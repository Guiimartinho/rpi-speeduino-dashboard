import QtQuick
import "../styles" as Styles

/**
 * VETable3D.qml - Visualizacao 3D real da tabela VE
 *
 * Baseado em como TunerStudio, FuelTech, MegaSquirt exibem mapas 3D:
 * - Eixo X: RPM (horizontal)
 * - Eixo Y: MAP/Load (profundidade)
 * - Eixo Z: VE% como ALTURA da superficie
 *
 * Caracteristicas:
 * - Superficie 3D continua (wireframe + preenchimento)
 * - Ponto vermelho mostrando posicao atual do motor
 * - Trail azul mostrando caminho recente
 * - Cores: Azul (baixo) -> Verde -> Amarelo -> Vermelho (alto)
 * - Rotacao interativa com mouse
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PROPRIEDADES PUBLICAS
    // ═══════════════════════════════════════════════════════════════════════

    property real rpm: 0
    property real mapKpa: 0
    property real veValue: 0

    property var rpmBins: [500, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 6000, 7000]
    property var mapBins: [20, 40, 60, 80, 100, 120, 140, 160, 180, 200]
    property var veData: generateDemoVE()

    property int gridSize: 10       // Aumentado para melhor resolução
    property real rotationX: 30      // Inclinacao (tilt) - aumentado
    property real rotationZ: -40     // Rotacao horizontal - ajustado
    property bool showWireframe: true
    property bool showSurface: true
    property bool showTrail: true
    property bool animated: true
    property bool showValues: false  // Compatibilidade com uso antigo

    // Tamanho minimo recomendado
    implicitWidth: 400
    implicitHeight: 350

    // ═══════════════════════════════════════════════════════════════════════
    // PROPRIEDADES CALCULADAS
    // ═══════════════════════════════════════════════════════════════════════

    readonly property int rpmIndex: findBinIndex(rpm, rpmBins)
    readonly property int mapIndex: findBinIndex(mapKpa, mapBins)

    // Dimensoes da projecao - otimizado para 800x480
    // Usa altura como base pois é o limitante em telas wide
    readonly property real baseSize: Math.min(width * 0.9, (height - 50) * 1.2)
    readonly property real surfaceWidth: baseSize * 0.95
    readonly property real surfaceDepth: surfaceWidth * 0.65
    readonly property real maxHeight: surfaceWidth * 0.5
    readonly property real cellWidth: surfaceWidth / gridSize
    readonly property real cellDepth: surfaceDepth / gridSize

    // Centro da superficie - removido offset
    readonly property real centerX: width / 2
    readonly property real centerY: height / 2

    // Trail history
    property var trailHistory: []
    property int trailLength: 30

    // ═══════════════════════════════════════════════════════════════════════
    // FUNCOES AUXILIARES
    // ═══════════════════════════════════════════════════════════════════════

    function findBinIndex(value, bins) {
        for (var i = bins.length - 1; i >= 0; i--) {
            if (value >= bins[i]) return Math.min(i, gridSize - 1)
        }
        return 0
    }

    function generateDemoVE() {
        var data = []
        for (var row = 0; row < 10; row++) {
            var rowData = []
            for (var col = 0; col < 10; col++) {
                // Simula curva VE tipica - pico no meio-alto RPM e alta carga
                var rpmFactor = Math.sin((col / 9) * Math.PI * 0.9) * 0.4 + 0.6
                var mapFactor = (row / 9) * 0.5 + 0.5
                var ve = 35 + rpmFactor * mapFactor * 65 + (Math.random() - 0.5) * 5
                rowData.push(Math.min(100, Math.max(30, ve)))
            }
            data.push(rowData)
        }
        return data
    }

    // Projeta ponto 3D para 2D com perspectiva isometrica
    function project3D(x, y, z) {
        // Normaliza coordenadas para -0.5 a 0.5
        var nx = (x / gridSize) - 0.5
        var ny = (y / gridSize) - 0.5
        var nz = z / 100  // VE normalizado (0-1)

        // Aplica rotacao Z (horizontal)
        var radZ = rotationZ * Math.PI / 180
        var rx = nx * Math.cos(radZ) - ny * Math.sin(radZ)
        var ry = nx * Math.sin(radZ) + ny * Math.cos(radZ)

        // Aplica rotacao X (tilt)
        var radX = rotationX * Math.PI / 180
        var rz = nz * maxHeight
        var projY = ry * Math.cos(radX) - (rz / maxHeight) * Math.sin(radX)
        var projZ = ry * Math.sin(radX) + (rz / maxHeight) * Math.cos(radX)

        // Escala e centraliza
        var screenX = centerX + rx * surfaceWidth
        var screenY = centerY - projY * surfaceDepth - projZ * maxHeight

        return { x: screenX, y: screenY, depth: ry + nz * 0.5 }
    }

    // Cor baseada no valor VE (gradiente profissional)
    function getVEColor(ve) {
        var t = (ve - 30) / 70  // Normaliza 30-100 para 0-1
        t = Math.max(0, Math.min(1, t))

        // Gradiente: Azul escuro -> Ciano -> Verde -> Amarelo -> Vermelho
        if (t < 0.25) {
            var s = t / 0.25
            return Qt.rgba(0, s * 0.8, 1 - s * 0.3, 0.85)  // Azul -> Ciano
        } else if (t < 0.5) {
            var s = (t - 0.25) / 0.25
            return Qt.rgba(0, 0.8 + s * 0.2, 0.7 - s * 0.7, 0.85)  // Ciano -> Verde
        } else if (t < 0.75) {
            var s = (t - 0.5) / 0.25
            return Qt.rgba(s, 1, 0, 0.85)  // Verde -> Amarelo
        } else {
            var s = (t - 0.75) / 0.25
            return Qt.rgba(1, 1 - s, 0, 0.85)  // Amarelo -> Vermelho
        }
    }

    function updateTrail() {
        if (!showTrail) return

        var newPoint = { rpmIdx: rpmIndex, mapIdx: mapIndex }

        if (trailHistory.length > 0) {
            var last = trailHistory[trailHistory.length - 1]
            if (last.rpmIdx === newPoint.rpmIdx && last.mapIdx === newPoint.mapIdx) {
                return
            }
        }

        trailHistory.push(newPoint)
        if (trailHistory.length > trailLength) {
            trailHistory.shift()
        }
        trailHistoryChanged()
        canvas.requestPaint()
    }

    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: {
            updateTrail()
            canvas.requestPaint()
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // INTERFACE VISUAL
    // ═══════════════════════════════════════════════════════════════════════

    Rectangle {
        anchors.fill: parent
        color: Styles.Theme.surfaceCard
        radius: Styles.Theme.radiusMedium
        border.color: Styles.Theme.backgroundElevated
        border.width: 1

        // Header
        Row {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Styles.Theme.spacingSm
            spacing: Styles.Theme.spacingMd

            Text {
                text: "VE TABLE 3D"
                color: Styles.Theme.accentSecondary
                font.pixelSize: Styles.Theme.fontMd
                font.weight: Font.Bold
            }

            Item { width: header.width - 300; height: 1 }  // Spacer

            // Toggle Wireframe
            Rectangle {
                width: 60
                height: 22
                radius: 4
                color: showWireframe ? "#00AAFF" : "#333333"
                Text {
                    anchors.centerIn: parent
                    text: "WIRE"
                    color: showWireframe ? "#000" : "#888"
                    font.pixelSize: 9
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        showWireframe = !showWireframe
                        canvas.requestPaint()
                    }
                }
            }

            // Valor VE atual
            Rectangle {
                width: 70
                height: 26
                radius: 6
                color: getVEColor(veValue)
                Text {
                    anchors.centerIn: parent
                    text: veValue.toFixed(0) + "%"
                    color: veValue > 70 ? "#000" : "#FFF"
                    font.pixelSize: Styles.Theme.fontMd
                    font.weight: Font.Bold
                    font.family: "Roboto Mono"
                }
            }
        }

        // Canvas para desenhar a superficie 3D
        Canvas {
            id: canvas
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.margins: 10
            anchors.rightMargin: 15  // Espaço extra para não cortar labels

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.clearRect(0, 0, width, height)

                // Coleta todos os quadrilateros com profundidade para ordenacao
                var quads = []

                for (var row = 0; row < gridSize - 1; row++) {
                    for (var col = 0; col < gridSize - 1; col++) {
                        // 4 cantos do quadrilatero
                        var ve00 = veData[row] ? (veData[row][col] || 50) : 50
                        var ve10 = veData[row] ? (veData[row][col + 1] || 50) : 50
                        var ve01 = veData[row + 1] ? (veData[row + 1][col] || 50) : 50
                        var ve11 = veData[row + 1] ? (veData[row + 1][col + 1] || 50) : 50

                        var p00 = project3D(col, row, ve00)
                        var p10 = project3D(col + 1, row, ve10)
                        var p01 = project3D(col, row + 1, ve01)
                        var p11 = project3D(col + 1, row + 1, ve11)

                        var avgVE = (ve00 + ve10 + ve01 + ve11) / 4
                        var avgDepth = (p00.depth + p10.depth + p01.depth + p11.depth) / 4

                        quads.push({
                            p00: p00, p10: p10, p01: p01, p11: p11,
                            ve: avgVE, depth: avgDepth,
                            row: row, col: col
                        })
                    }
                }

                // Ordena por profundidade (desenha de tras para frente)
                quads.sort(function(a, b) { return a.depth - b.depth })

                // Desenha cada quadrilatero
                for (var i = 0; i < quads.length; i++) {
                    var q = quads[i]

                    // Superficie preenchida
                    if (showSurface) {
                        ctx.beginPath()
                        ctx.moveTo(q.p00.x, q.p00.y)
                        ctx.lineTo(q.p10.x, q.p10.y)
                        ctx.lineTo(q.p11.x, q.p11.y)
                        ctx.lineTo(q.p01.x, q.p01.y)
                        ctx.closePath()

                        var color = getVEColor(q.ve)
                        ctx.fillStyle = Qt.rgba(color.r, color.g, color.b, 0.7)
                        ctx.fill()
                    }

                    // Wireframe
                    if (showWireframe) {
                        ctx.beginPath()
                        ctx.moveTo(q.p00.x, q.p00.y)
                        ctx.lineTo(q.p10.x, q.p10.y)
                        ctx.lineTo(q.p11.x, q.p11.y)
                        ctx.lineTo(q.p01.x, q.p01.y)
                        ctx.closePath()
                        ctx.strokeStyle = "rgba(255, 255, 255, 0.3)"
                        ctx.lineWidth = 0.5
                        ctx.stroke()
                    }
                }

                // Desenha trail (caminho recente)
                if (showTrail && trailHistory.length > 1) {
                    ctx.beginPath()
                    for (var t = 0; t < trailHistory.length; t++) {
                        var tp = trailHistory[t]
                        var tve = veData[tp.mapIdx] ? (veData[tp.mapIdx][tp.rpmIdx] || 50) : 50
                        var tproj = project3D(tp.rpmIdx + 0.5, tp.mapIdx + 0.5, tve)

                        if (t === 0) {
                            ctx.moveTo(tproj.x, tproj.y)
                        } else {
                            ctx.lineTo(tproj.x, tproj.y)
                        }
                    }
                    ctx.strokeStyle = "rgba(0, 170, 255, 0.6)"
                    ctx.lineWidth = 2
                    ctx.stroke()

                    // Pontos do trail
                    for (var t = 0; t < trailHistory.length; t++) {
                        var tp = trailHistory[t]
                        var tve = veData[tp.mapIdx] ? (veData[tp.mapIdx][tp.rpmIdx] || 50) : 50
                        var tproj = project3D(tp.rpmIdx + 0.5, tp.mapIdx + 0.5, tve)
                        var alpha = 0.3 + (t / trailHistory.length) * 0.5

                        ctx.beginPath()
                        ctx.arc(tproj.x, tproj.y, 3, 0, Math.PI * 2)
                        ctx.fillStyle = "rgba(0, 170, 255, " + alpha + ")"
                        ctx.fill()
                    }
                }

                // Desenha posicao atual (ponto vermelho brilhante)
                var currentVE = veData[mapIndex] ? (veData[mapIndex][rpmIndex] || 50) : 50
                var currentPos = project3D(rpmIndex + 0.5, mapIndex + 0.5, currentVE)

                // Glow effect
                var gradient = ctx.createRadialGradient(
                    currentPos.x, currentPos.y, 0,
                    currentPos.x, currentPos.y, 20
                )
                gradient.addColorStop(0, "rgba(255, 50, 50, 0.8)")
                gradient.addColorStop(0.5, "rgba(255, 50, 50, 0.3)")
                gradient.addColorStop(1, "rgba(255, 50, 50, 0)")

                ctx.beginPath()
                ctx.arc(currentPos.x, currentPos.y, 20, 0, Math.PI * 2)
                ctx.fillStyle = gradient
                ctx.fill()

                // Ponto central
                ctx.beginPath()
                ctx.arc(currentPos.x, currentPos.y, 6, 0, Math.PI * 2)
                ctx.fillStyle = "#FF3333"
                ctx.fill()
                ctx.strokeStyle = "#FFFFFF"
                ctx.lineWidth = 2
                ctx.stroke()

                // Linha vertical ate a base (mostra altura)
                var basePos = project3D(rpmIndex + 0.5, mapIndex + 0.5, 30)
                ctx.beginPath()
                ctx.moveTo(currentPos.x, currentPos.y)
                ctx.lineTo(basePos.x, basePos.y)
                ctx.strokeStyle = "rgba(255, 100, 100, 0.5)"
                ctx.lineWidth = 1
                ctx.setLineDash([3, 3])
                ctx.stroke()
                ctx.setLineDash([])

                // Labels dos eixos
                ctx.fillStyle = "rgba(255, 255, 255, 0.7)"
                ctx.font = "bold 11px sans-serif"

                // RPM label
                var rpmLabelPos = project3D(gridSize / 2, -0.8, 30)
                ctx.fillText("RPM →", rpmLabelPos.x - 20, rpmLabelPos.y)

                // MAP label
                var mapLabelPos = project3D(-0.8, gridSize / 2, 30)
                ctx.fillText("MAP ↗", mapLabelPos.x - 10, mapLabelPos.y)

                // VE label
                ctx.fillText("VE%", 15, centerY - maxHeight * 0.8)
            }
        }

        // Footer com dados e legenda horizontal
        Row {
            id: footer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Styles.Theme.spacingSm
            spacing: Styles.Theme.spacingMd

            Text {
                text: "RPM: " + rpm.toFixed(0)
                color: Styles.Theme.textSecondary
                font.pixelSize: Styles.Theme.fontSm
                font.family: "Roboto Mono"
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "MAP: " + mapKpa.toFixed(0)
                color: Styles.Theme.textSecondary
                font.pixelSize: Styles.Theme.fontSm
                font.family: "Roboto Mono"
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "[" + rpmIndex + "," + mapIndex + "]"
                color: Styles.Theme.textTertiary
                font.pixelSize: Styles.Theme.fontXs
                font.family: "Roboto Mono"
                anchors.verticalCenter: parent.verticalCenter
            }

            // Spacer
            Item { width: 10; height: 1 }

            // Legenda horizontal compacta
            Row {
                spacing: 3
                anchors.verticalCenter: parent.verticalCenter

                Repeater {
                    model: [30, 50, 70, 85, 100]

                    Row {
                        spacing: 2
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 2
                            color: getVEColor(modelData)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: modelData
                            color: Styles.Theme.textTertiary
                            font.pixelSize: 8
                            font.family: "Roboto Mono"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }

    // Controle de rotacao com mouse
    MouseArea {
        anchors.fill: parent
        property real lastX: 0
        property real lastY: 0

        onPressed: {
            lastX = mouseX
            lastY = mouseY
        }

        onPositionChanged: {
            if (pressed) {
                var dx = mouseX - lastX
                var dy = mouseY - lastY

                rotationZ = rotationZ + dx * 0.5
                rotationX = Math.max(5, Math.min(60, rotationX + dy * 0.3))

                lastX = mouseX
                lastY = mouseY
                canvas.requestPaint()
            }
        }

        onDoubleClicked: {
            // Reset para vista padrao
            rotationX = 25
            rotationZ = -35
            canvas.requestPaint()
        }
    }
}
