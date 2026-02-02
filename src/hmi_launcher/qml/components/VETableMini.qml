import QtQuick
import "../styles" as Styles

/**
 * VETableMini.qml - Mini-preview da tabela VE para o dashboard
 *
 * Versão compacta do VEMapDisplay para uso no layout TUNING.
 * Mostra uma grade 8x8 com cursor animado e trail de posições.
 *
 * Características:
 * - Grade 8x8 (ou 16x16 configurável)
 * - Cursor mostrando posição atual (RPM x MAP)
 * - Trail azul mostrando caminho dos últimos segundos
 * - Cores baseadas nos valores VE
 * - Auto-scroll para manter cursor visível
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PROPRIEDADES PÚBLICAS
    // ═══════════════════════════════════════════════════════════════════════

    // Ponto de operação atual
    property real rpm: 0
    property real mapKpa: 0
    property real veValue: 0

    // Bins do eixo (valores padrão Speeduino)
    property var rpmBins: [500, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 6000, 7000]
    property var mapBins: [20, 40, 60, 80, 100, 120, 140, 160, 180, 200]

    // Dados da tabela VE (10x10 demo)
    property var veData: generateDemoVE()

    // Opções visuais
    property int gridSize: 8
    property bool showValues: false
    property bool showTrail: true
    property bool showLabels: true
    property int trailLength: 20

    // Tamanho dinâmico - preenche o container pai
    implicitWidth: 300
    implicitHeight: 300

    // ═══════════════════════════════════════════════════════════════════════
    // PROPRIEDADES CALCULADAS
    // ═══════════════════════════════════════════════════════════════════════

    // Índices atuais na tabela
    readonly property int rpmIndex: findBinIndex(rpm, rpmBins)
    readonly property int mapIndex: findBinIndex(mapKpa, mapBins)

    // Histórico de posições para o trail
    property var trailHistory: []

    // ═══════════════════════════════════════════════════════════════════════
    // FUNÇÕES AUXILIARES
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
                // Simula valores VE típicos (30-100%)
                var ve = 40 + (row * 4) + (col * 3) + Math.random() * 8
                rowData.push(Math.min(100, Math.max(30, ve)))
            }
            data.push(rowData)
        }
        return data
    }

    function getVEColor(ve) {
        // Gradiente 4-etapas igual ao VETable3D: Azul → Ciano → Verde → Amarelo → Vermelho
        var t = (ve - 30) / 70  // Normaliza 30-100 para 0-1
        t = Math.max(0, Math.min(1, t))

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

        // Evita duplicatas consecutivas
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
    }

    // Timer para atualizar trail
    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: updateTrail()
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ESTRUTURA VISUAL
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
            anchors.margins: Styles.Theme.spacingXs
            spacing: Styles.Theme.spacingMd

            Text {
                text: "VE TABLE"
                color: Styles.Theme.accentPrimary
                font.pixelSize: Styles.Theme.fontSm
                font.weight: Font.Bold
            }

            // Spacer
            Item { width: header.width - 120; height: 1 }

            Text {
                text: veValue.toFixed(0) + "%"
                color: Styles.Theme.textPrimary
                font.pixelSize: Styles.Theme.fontMd
                font.weight: Font.Bold
                font.family: "Roboto Mono"
            }
        }

        // Container da grade
        Item {
            id: gridContainer
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.margins: Styles.Theme.spacingXs

            // Labels do eixo MAP (esquerda)
            Column {
                id: mapLabels
                visible: showLabels
                anchors.left: parent.left
                anchors.top: rpmLabels.bottom
                anchors.bottom: parent.bottom
                width: Styles.Theme.dp(36)  // Aumentado de 24 para 36

                Repeater {
                    model: gridSize

                    Text {
                        width: parent.width
                        height: (gridContainer.height - rpmLabels.height) / gridSize
                        text: mapBins[gridSize - 1 - index] || ""
                        color: (gridSize - 1 - index) === mapIndex ? Styles.Theme.accentPrimary : Styles.Theme.textTertiary
                        font.pixelSize: Styles.Theme.fontXs
                        font.family: "Roboto Mono"
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // Labels do eixo RPM (topo)
            Row {
                id: rpmLabels
                visible: showLabels
                anchors.left: mapLabels.right
                anchors.leftMargin: Styles.Theme.spacingXs
                anchors.top: parent.top
                anchors.right: parent.right
                height: Styles.Theme.dp(14)

                Repeater {
                    model: gridSize

                    Text {
                        width: (gridContainer.width - mapLabels.width - Styles.Theme.spacingXs) / gridSize
                        height: parent.height
                        text: rpmBins[index] ? (rpmBins[index] / 1000).toFixed(1) : ""
                        color: index === rpmIndex ? Styles.Theme.accentPrimary : Styles.Theme.textTertiary
                        font.pixelSize: Styles.Theme.fontXs
                        font.family: "Roboto Mono"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            // Grade de células
            Grid {
                id: veGrid
                anchors.left: mapLabels.right
                anchors.leftMargin: Styles.Theme.spacingXs
                anchors.top: rpmLabels.bottom
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                columns: gridSize
                rows: gridSize

                Repeater {
                    model: gridSize * gridSize

                    Rectangle {
                        id: cell
                        property int colIdx: index % gridSize
                        property int rowIdx: gridSize - 1 - Math.floor(index / gridSize)
                        property bool isCurrent: colIdx === rpmIndex && rowIdx === mapIndex
                        property real cellVE: veData[rowIdx] ? (veData[rowIdx][colIdx] || 50) : 50

                        width: (gridContainer.width - mapLabels.width - Styles.Theme.spacingXs) / gridSize
                        height: (gridContainer.height - rpmLabels.height) / gridSize
                        color: getVEColor(cellVE)
                        border.color: isCurrent ? Styles.Theme.textPrimary : Styles.Theme.backgroundPrimary
                        border.width: isCurrent ? 2 : 1

                        // Trail highlight
                        Rectangle {
                            anchors.fill: parent
                            color: Styles.Theme.accentSecondary
                            opacity: {
                                if (!showTrail) return 0
                                for (var i = 0; i < trailHistory.length; i++) {
                                    if (trailHistory[i].rpmIdx === cell.colIdx &&
                                        trailHistory[i].mapIdx === cell.rowIdx) {
                                        return 0.3 + (i / trailHistory.length) * 0.4
                                    }
                                }
                                return 0
                            }
                            visible: opacity > 0
                        }

                        // Cursor atual
                        Rectangle {
                            visible: isCurrent
                            anchors.fill: parent
                            anchors.margins: 2
                            color: "transparent"
                            border.color: Styles.Theme.textPrimary
                            border.width: 2
                            radius: 2

                            // Animação de pulso
                            SequentialAnimation on opacity {
                                running: isCurrent
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.5; duration: 400 }
                                NumberAnimation { to: 1.0; duration: 400 }
                            }
                        }

                        // Valor VE
                        Text {
                            visible: showValues
                            anchors.centerIn: parent
                            text: cellVE.toFixed(0)
                            color: isCurrent ? Styles.Theme.backgroundPrimary : Styles.Theme.textPrimary
                            font.pixelSize: Math.min(cell.width, cell.height) * 0.4
                            font.weight: isCurrent ? Font.Bold : Font.Normal
                            font.family: "Roboto Mono"
                        }
                    }
                }
            }
        }

        // Footer com coordenadas e legenda
        Row {
            id: footer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Styles.Theme.spacingXs
            spacing: Styles.Theme.spacingSm

            Text {
                text: "RPM: " + rpm.toFixed(0)
                color: Styles.Theme.textSecondary
                font.pixelSize: Styles.Theme.fontXs
                font.family: "Roboto Mono"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "MAP: " + mapKpa.toFixed(0)
                color: Styles.Theme.textSecondary
                font.pixelSize: Styles.Theme.fontXs
                font.family: "Roboto Mono"
                anchors.verticalCenter: parent.verticalCenter
            }

            // Spacer
            Item { width: 5; height: 1 }

            // Legenda compacta
            Row {
                spacing: 2
                anchors.verticalCenter: parent.verticalCenter

                Repeater {
                    model: [30, 65, 100]

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 2
                        color: getVEColor(modelData)
                    }
                }
            }
        }
    }
}
