import QtQuick
import "../styles" as Styles

/**
 * ProgressiveShiftBar.qml - Barra de progresso estilo download para shift
 *
 * Inspirado em: FuelTech FT600, OpenAuto Pro
 * Substitui LEDs discretos por barra gradiente contínua.
 *
 * Características:
 * - Preenchimento gradiente contínuo (não LEDs)
 * - Progressão de cor: verde → amarelo → laranja → vermelho
 * - Animação de flash no ponto de troca
 * - Threshold de shift RPM configurável
 * - Animações suaves para uso automotivo
 *
 * Configuração via propriedades:
 *   rpm: Valor atual de RPM
 *   minRpm: RPM onde a barra começa (padrão: 1000)
 *   shiftRpm: RPM onde a barra está 100% (ponto de troca)
 */
Item {
    id: root

    // ═══════════════════════════════════════════════════════════════════════
    // PROPRIEDADES PÚBLICAS
    // ═══════════════════════════════════════════════════════════════════════

    property real rpm: 0
    property real minRpm: 1000
    property real shiftRpm: 6500

    // Opções visuais
    property bool showPercentage: false
    property bool showRpmValue: false
    property bool showGlow: true
    property bool roundedCaps: true
    property bool showShiftLabel: true
    property bool showDividers: true

    // Thresholds de cor (percentual do preenchimento)
    property real yellowThreshold: 0.60
    property real orangeThreshold: 0.80
    property real redThreshold: 0.95

    // Cores (paleta do HMI_ROADMAP.md)
    property color greenColor: "#00FF00"
    property color yellowColor: "#FFFF00"
    property color orangeColor: "#FF8800"
    property color redColor: "#FF0000"
    property color backgroundColor: Styles.Theme.backgroundTertiary
    property color borderColor: Styles.Theme.backgroundElevated

    // Dimensões
    property int barHeight: Styles.Theme.dp(14)
    property int barRadius: roundedCaps ? barHeight / 2 : Styles.Theme.radiusSmall

    // Tamanho padrão
    width: parent ? parent.width : Styles.Theme.dp(400)
    height: barHeight + (showPercentage || showRpmValue ? Styles.Theme.dp(18) : 0)

    // ═══════════════════════════════════════════════════════════════════════
    // PROPRIEDADES CALCULADAS
    // ═══════════════════════════════════════════════════════════════════════

    readonly property real fillPercent: {
        if (rpm <= minRpm) return 0.0
        if (rpm >= shiftRpm) return 1.0
        return (rpm - minRpm) / (shiftRpm - minRpm)
    }

    readonly property int displayPercent: Math.round(fillPercent * 100)

    readonly property bool shouldFlash: fillPercent >= 0.98

    readonly property color currentColor: {
        if (fillPercent < yellowThreshold) return greenColor
        if (fillPercent < orangeThreshold) return yellowColor
        if (fillPercent < redThreshold) return orangeColor
        return redColor
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ANIMAÇÃO DE FLASH
    // ═══════════════════════════════════════════════════════════════════════

    property bool flashState: true

    Timer {
        id: flashTimer
        interval: 80
        running: shouldFlash
        repeat: true
        onTriggered: flashState = !flashState
    }

    onShouldFlashChanged: {
        if (!shouldFlash) flashState = true
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ESTRUTURA VISUAL
    // ═══════════════════════════════════════════════════════════════════════

    // Barra de fundo (track)
    Rectangle {
        id: trackBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: barHeight
        radius: barRadius
        color: backgroundColor
        border.color: borderColor
        border.width: 1

        // Barra de preenchimento (progresso)
        Rectangle {
            id: fillBar
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 1
            width: Math.max(0, (parent.width - 2) * fillPercent)
            radius: barRadius - 1

            // Gradiente para transição suave de cor
            gradient: Gradient {
                orientation: Gradient.Horizontal

                GradientStop { position: 0.0; color: greenColor }
                GradientStop { position: yellowThreshold; color: yellowColor }
                GradientStop { position: orangeThreshold; color: orangeColor }
                GradientStop { position: 1.0; color: redColor }
            }

            // Overlay de flash no ponto de troca
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "white"
                opacity: shouldFlash && flashState ? 0.4 : 0.0
                visible: shouldFlash

                Behavior on opacity {
                    NumberAnimation { duration: 40 }
                }
            }

            // Animação suave de largura
            Behavior on width {
                NumberAnimation {
                    duration: Styles.Theme.animationFast
                    easing.type: Easing.OutQuad
                }
            }
        }

        // Efeito de glow quando ativo
        Rectangle {
            visible: showGlow && fillPercent > 0.5
            anchors.fill: parent
            anchors.margins: -Styles.Theme.dp(3)
            radius: barRadius + Styles.Theme.dp(3)
            color: "transparent"
            border.color: currentColor
            border.width: Styles.Theme.dp(2)
            opacity: shouldFlash ? (flashState ? 0.6 : 0.2) : (fillPercent - 0.5) * 0.6

            Behavior on opacity {
                NumberAnimation { duration: Styles.Theme.animationFast }
            }
        }

        // Marcas de referência visual (cada 20%)
        Row {
            visible: showDividers
            anchors.fill: parent
            anchors.leftMargin: parent.width * 0.2
            anchors.rightMargin: 0
            spacing: (parent.width * 0.8 - 4) / 4

            Repeater {
                model: 4

                Rectangle {
                    width: 1
                    height: parent.height
                    color: Styles.Theme.textTertiary
                    opacity: 0.2
                }
            }
        }
    }

    // Label "SHIFT!" quando no redline
    Rectangle {
        id: shiftLabel
        visible: showShiftLabel && shouldFlash
        anchors.centerIn: trackBar
        width: Styles.Theme.dp(55)
        height: Styles.Theme.dp(18)
        radius: Styles.Theme.radiusSmall
        color: flashState ? redColor : "#AA0000"

        Text {
            anchors.centerIn: parent
            text: "SHIFT!"
            color: Styles.Theme.textPrimary
            font.pixelSize: Styles.Theme.fontSm
            font.weight: Font.Bold
        }

        scale: shouldFlash ? 1.0 : 0.8
        opacity: shouldFlash ? 1.0 : 0.0

        Behavior on scale {
            NumberAnimation {
                duration: Styles.Theme.animationFast
                easing.type: Easing.OutBack
            }
        }

        Behavior on opacity {
            NumberAnimation { duration: Styles.Theme.animationFast }
        }
    }

    // Linha de percentual/RPM
    Row {
        visible: showPercentage || showRpmValue
        anchors.top: trackBar.bottom
        anchors.topMargin: Styles.Theme.spacingXs
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Styles.Theme.spacingMd

        Text {
            visible: showPercentage
            text: displayPercent + "%"
            color: shouldFlash ? redColor : currentColor
            font.pixelSize: Styles.Theme.fontSm
            font.weight: Font.Bold
            font.family: "Roboto Mono"

            Behavior on color {
                ColorAnimation { duration: Styles.Theme.animationFast }
            }
        }

        Text {
            visible: showPercentage && showRpmValue
            text: "|"
            color: Styles.Theme.textTertiary
            font.pixelSize: Styles.Theme.fontSm
        }

        Text {
            visible: showRpmValue
            text: Math.round(rpm) + " RPM"
            color: shouldFlash ? redColor : Styles.Theme.textSecondary
            font.pixelSize: Styles.Theme.fontSm
            font.family: "Roboto Mono"
        }
    }
}
