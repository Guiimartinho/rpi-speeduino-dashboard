import QtQuick
import QtQuick.Layouts
import "../styles" as Styles

/**
 * AFRHistogram.qml - Histograma dual AFR (Atual vs Target)
 *
 * Mostra barras horizontais comparando:
 * - AFR Atual (leitura do sensor wideband)
 * - AFR Target (valor da tabela da ECU)
 *
 * Visual inspirado em: TunerStudio, FuelTech FT600
 *
 * Cores:
 * - Verde: AFR dentro da faixa ideal
 * - Amarelo: AFR ligeiramente fora
 * - Vermelho: AFR perigoso (muito rico ou muito pobre)
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PROPRIEDADES PÚBLICAS
    // ═══════════════════════════════════════════════════════════════════════

    property real afrActual: 14.7
    property real afrTarget: 14.7

    // Faixa de exibição
    property real minAfr: 10.0
    property real maxAfr: 18.0

    // Thresholds de aviso
    property real leanWarning: 15.5    // Muito pobre
    property real leanDanger: 16.5     // Perigoso pobre
    property real richWarning: 12.0    // Muito rico
    property real richDanger: 10.5     // Perigoso rico

    // Opções visuais
    property bool showLabels: true
    property bool showDifference: true
    property bool compactMode: false

    // Dimensões
    property int barHeight: compactMode ? Styles.Theme.dp(10) : Styles.Theme.dp(14)

    // Tamanho padrão
    width: parent ? parent.width : Styles.Theme.dp(300)
    height: compactMode ? Styles.Theme.dp(50) : Styles.Theme.dp(70)

    // ═══════════════════════════════════════════════════════════════════════
    // PROPRIEDADES CALCULADAS
    // ═══════════════════════════════════════════════════════════════════════

    readonly property real afrDifference: afrActual - afrTarget

    readonly property real actualPercent: {
        return Math.max(0, Math.min(1, (afrActual - minAfr) / (maxAfr - minAfr)))
    }

    readonly property real targetPercent: {
        return Math.max(0, Math.min(1, (afrTarget - minAfr) / (maxAfr - minAfr)))
    }

    readonly property color actualColor: {
        if (afrActual >= leanDanger) return "#FF0000"
        if (afrActual >= leanWarning) return "#FF8800"
        if (afrActual <= richDanger) return "#FF0000"
        if (afrActual <= richWarning) return "#FF8800"
        return "#00FF00"
    }

    readonly property color targetColor: "#00AAFF"

    readonly property string differenceText: {
        var diff = afrDifference
        var sign = diff >= 0 ? "+" : ""
        return sign + diff.toFixed(2)
    }

    readonly property color differenceColor: {
        var absDiff = Math.abs(afrDifference)
        if (absDiff > 1.0) return "#FF0000"
        if (absDiff > 0.5) return "#FF8800"
        if (absDiff > 0.2) return "#FFFF00"
        return "#00FF00"
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

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Styles.Theme.spacingSm
            spacing: Styles.Theme.spacingXs

            // Header com valores
            RowLayout {
                Layout.fillWidth: true
                spacing: Styles.Theme.spacingMd

                // AFR Atual
                Row {
                    spacing: Styles.Theme.spacingXs

                    Text {
                        text: "AFR:"
                        color: Styles.Theme.textSecondary
                        font.pixelSize: compactMode ? Styles.Theme.fontSm : Styles.Theme.fontMd
                        font.weight: Font.Bold
                    }

                    Text {
                        text: afrActual.toFixed(1)
                        color: actualColor
                        font.pixelSize: compactMode ? Styles.Theme.fontMd : Styles.Theme.fontLg
                        font.weight: Font.Bold
                        font.family: "Roboto Mono"
                    }
                }

                // Separador
                Rectangle {
                    width: 1
                    height: Styles.Theme.fontLg
                    color: Styles.Theme.textTertiary
                    opacity: 0.3
                }

                // Target
                Row {
                    spacing: Styles.Theme.spacingXs

                    Text {
                        text: "TGT:"
                        color: Styles.Theme.textSecondary
                        font.pixelSize: compactMode ? Styles.Theme.fontSm : Styles.Theme.fontMd
                        font.weight: Font.Bold
                    }

                    Text {
                        text: afrTarget.toFixed(1)
                        color: targetColor
                        font.pixelSize: compactMode ? Styles.Theme.fontMd : Styles.Theme.fontLg
                        font.weight: Font.Bold
                        font.family: "Roboto Mono"
                    }
                }

                Item { Layout.fillWidth: true }

                // Diferença
                Row {
                    visible: showDifference
                    spacing: Styles.Theme.spacingXs

                    Text {
                        text: "Δ"
                        color: Styles.Theme.textTertiary
                        font.pixelSize: compactMode ? Styles.Theme.fontSm : Styles.Theme.fontMd
                    }

                    Text {
                        text: differenceText
                        color: differenceColor
                        font.pixelSize: compactMode ? Styles.Theme.fontMd : Styles.Theme.fontLg
                        font.weight: Font.Bold
                        font.family: "Roboto Mono"
                    }
                }
            }

            // Barra AFR Atual
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: barHeight

                // Background
                Rectangle {
                    anchors.fill: parent
                    radius: barHeight / 2
                    color: Styles.Theme.backgroundTertiary
                    border.color: Styles.Theme.backgroundElevated
                    border.width: 1

                    // Fill bar
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 1
                        width: Math.max(0, (parent.width - 2) * actualPercent)
                        radius: (barHeight - 2) / 2
                        color: actualColor

                        Behavior on width {
                            NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
                        }

                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }

                    // Marcador de Target
                    Rectangle {
                        x: (parent.width - 2) * targetPercent - width / 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: parent.height + 4
                        radius: 1
                        color: targetColor
                        border.color: Styles.Theme.backgroundPrimary
                        border.width: 1

                        Behavior on x {
                            NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
                        }
                    }
                }

                // Label
                Text {
                    visible: showLabels && !compactMode
                    anchors.left: parent.left
                    anchors.bottom: parent.top
                    anchors.bottomMargin: 2
                    text: "ACTUAL"
                    color: Styles.Theme.textTertiary
                    font.pixelSize: Styles.Theme.fontXs
                    font.letterSpacing: 1
                }
            }

            // Barra AFR Target (modo não compacto)
            Item {
                visible: !compactMode
                Layout.fillWidth: true
                Layout.preferredHeight: barHeight

                Rectangle {
                    anchors.fill: parent
                    radius: barHeight / 2
                    color: Styles.Theme.backgroundTertiary
                    border.color: Styles.Theme.backgroundElevated
                    border.width: 1

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 1
                        width: Math.max(0, (parent.width - 2) * targetPercent)
                        radius: (barHeight - 2) / 2
                        color: targetColor
                        opacity: 0.7

                        Behavior on width {
                            NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
                        }
                    }
                }

                Text {
                    visible: showLabels
                    anchors.left: parent.left
                    anchors.bottom: parent.top
                    anchors.bottomMargin: 2
                    text: "TARGET"
                    color: Styles.Theme.textTertiary
                    font.pixelSize: Styles.Theme.fontXs
                    font.letterSpacing: 1
                }
            }

            // Escala de referência
            Row {
                id: scaleRow
                visible: showLabels && !compactMode
                Layout.fillWidth: true
                spacing: 0

                Repeater {
                    model: 5

                    Text {
                        width: scaleRow.width / 5
                        text: (minAfr + index * (maxAfr - minAfr) / 4).toFixed(1)
                        color: Styles.Theme.textTertiary
                        font.pixelSize: Styles.Theme.fontXs
                        font.family: "Roboto Mono"
                        horizontalAlignment: index === 0 ? Text.AlignLeft :
                                            index === 4 ? Text.AlignRight : Text.AlignHCenter
                    }
                }
            }
        }
    }
}
