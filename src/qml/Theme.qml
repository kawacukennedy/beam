import QtQuick
import QtGraphicalEffects // Import for DropShadow

// Theme.qml - Defines global UI styles

QtObject {
    id: theme

    // Grid Unit
    readonly property int gridUnit: 8

    // Corner Radius
    readonly property int cornerRadius: 12

    // Shadows (implemented with DropShadow)
    readonly property var shadows: {
        "low": DropShadow {
            anchors.fill: parent
            horizontalOffset: 0
            verticalOffset: 1
            radius: 3
            samples: 17
            color: "#1F000000" // rgba(0,0,0,0.12) converted to ARGB hex
            spread: 0.12
        },
        "medium": DropShadow {
            anchors.fill: parent
            horizontalOffset: 0
            verticalOffset: 4
            radius: 6
            samples: 17
            color: "#29000000" // rgba(0,0,0,0.16) converted to ARGB hex
            spread: 0.16
        },
        "high": DropShadow {
            anchors.fill: parent
            horizontalOffset: 0
            verticalOffset: 10
            radius: 20
            samples: 17
            color: "#40000000" // rgba(0,0,0,0.25) converted to ARGB hex
            spread: 0.25
        }
    }

    // Fonts
    readonly property var fonts: {
        "family": System.font.family,
        "h1": 24,
        "h2": 18,
        "body": 14,
        "caption": 12,
        "h1Weight": Font.DemiBold,
        "h2Weight": Font.Medium,
        "bodyWeight": Font.Normal,
        "captionWeight": Font.Normal
    }

    // Colors (Light and Dark themes)
    readonly property var colors: {
        "light": {
            "background": "#FAFAFA",
            "surface": "#FFFFFF",
            "primary": "#0078D7",
            "secondary": "#00A1F1",
            "text_primary": "#1C1C1C",
            "text_secondary": "#666666",
            "border": "#E0E0E0",
            "error": "#D32F2F",
            "success": "#2E7D32",
            "sidebar_hover": "#E6F0FF"
        },
        "dark": {
            "background": "#1E1E1E",
            "surface": "#2C2C2C",
            "primary": "#0A84FF",
            "secondary": "#0A9EFF",
            "text_primary": "#FFFFFF",
            "text_secondary": "#AAAAAA",
            "border": "#333333",
            "error": "#FF6B6B",
            "success": "#4CAF50",
            "sidebar_hover": "#2A2A2A"
        }
    }

    // Current theme (default to light)
    property string currentTheme: "light"

    // Helper to get current color
    function color(key) {
        return theme.colors[theme.currentTheme][key];
    }
}
