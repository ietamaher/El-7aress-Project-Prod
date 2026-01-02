pragma Singleton
import QtQuick

/**
 * OverlayTheme.qml - Unified theming constants for all overlay components
 *
 * Reference: BrightnessOverlay.qml
 * All overlays must conform to these values for visual consistency.
 *
 * Categories:
 * - Standard Overlays (fixed-size panels): BrightnessOverlay, WindageOverlay,
 *   ZeroingOverlay, PresetHomePositionOverlay, EnvironmentalOverlay
 * - Modal Dialogs (full-screen with backdrop): AboutDialog, ShutdownConfirmationDialog
 */
QtObject {
    // =========================================================================
    // COLORS - Core palette
    // =========================================================================

    // Background colors
    readonly property color backgroundColor: "#0A0A0A"       // Main panel background
    readonly property color headerBackground: "transparent"  // Header section
    readonly property color footerBackground: "#0D0D0D"      // Footer section
    readonly property color separatorColor: "#151515"        // Section dividers
    readonly property color borderColor: "#1A1A1A"           // Panel border

    // Modal dialog colors
    readonly property color modalBackdrop: "#CC000000"       // 80% opacity black
    readonly property color modalPanelBackground: "#0A0A0A"  // Same as standard panels

    // Content panel colors (accent-tinted)
    readonly property real contentPanelBgOpacity: 0.05       // For Qt.rgba with accent
    readonly property real contentPanelBorderOpacity: 0.3    // For Qt.rgba with accent
    readonly property color contentPanelSolidBg: "#0F0F0F"   // Alternative solid background

    // Text colors
    readonly property color textPrimary: "#FFFFFF"           // Titles, headers
    readonly property color textSecondary: "#CCCCCC"         // Instructions, body
    readonly property color textTertiary: "#AAAAAA"          // Labels
    readonly property color textMuted: "#606060"             // Footer hints
    readonly property color textSubtle: "#666666"            // Very subtle text

    // Accent & status colors
    readonly property color accentDefault: "#46E2A5"         // Default accent (green)
    readonly property color dangerColor: "#FF4444"           // Warning/danger (red)
    readonly property color dividerLight: "#252525"          // Light divider in content
    readonly property color dividerMedium: "#333333"         // Medium divider
    readonly property color inactiveIndicator: "#333333"     // Inactive state

    // =========================================================================
    // TYPOGRAPHY - Font settings
    // =========================================================================

    readonly property string fontFamily: "Segoe UI"
    readonly property string fontFamilyMono: "Segoe UI Mono"
    readonly property var fontFamilyMonoFallback: ["Segoe UI Mono", "Consolas", "Segoe UI"]

    // Font sizes
    readonly property int fontSizeTitle: 22           // Panel titles
    readonly property int fontSizeHeading: 20         // Parameter labels, status
    readonly property int fontSizeBody: 14            // Instructions, body text
    readonly property int fontSizeLabel: 13           // Field labels
    readonly property int fontSizeFooter: 12          // Footer hints
    readonly property int fontSizeSmall: 11           // Very small text
    readonly property int fontSizeStatus: 18          // Status text (wind direction, etc.)
    readonly property int fontSizeStatusLarge: 16     // Large status text

    // Font weights
    readonly property int fontWeightNormal: Font.Normal
    readonly property int fontWeightDemiBold: Font.DemiBold
    readonly property int fontWeightBold: Font.Bold

    // =========================================================================
    // DIMENSIONS - Layout measurements
    // =========================================================================

    // Panel dimensions (standard overlays)
    readonly property int panelWidth: 500
    readonly property int panelHeight: 350            // Reference height (content may vary)
    readonly property int panelHeightLarge: 400       // For overlays with more content

    // Section heights
    readonly property int headerHeight: 80
    readonly property int footerHeight: 50
    readonly property int separatorHeight: 1

    // Content area
    readonly property int contentMargin: 30           // (width - 60) / 2
    readonly property int contentPadding: 80          // For instruction text width
    readonly property int contentSpacing: 10          // Default spacing in content
    readonly property int contentSpacingLarge: 15     // Larger spacing variant

    // Panel styling
    readonly property int panelRadius: 0              // No rounded corners
    readonly property int panelBorderWidth: 1
    readonly property int contentPanelRadius: 5       // Rounded content panels

    // Value display boxes
    readonly property int valueBoxHeight: 24
    readonly property int valueBoxHeightLarge: 28
    readonly property int valueBoxRadius: 3
    readonly property int valueBoxWidth: 100
    readonly property int valueBoxWidthLarge: 140

    // Progress bar
    readonly property int progressBarWidth: 300
    readonly property int progressBarHeight: 8
    readonly property int progressBarRadius: 4

    // Indicator bars
    readonly property int indicatorWidth: 4
    readonly property int indicatorHeight: 12
    readonly property int indicatorRadius: 2
    readonly property int indicatorSpacing: 26

    // Header content spacing
    readonly property int headerSpacing: 8

    // Row spacing for data displays
    readonly property int dataRowSpacing: 20
    readonly property int labelWidth: 120

    // =========================================================================
    // SHADOW - Drop shadow settings for layer.effect
    // =========================================================================

    readonly property bool shadowEnabled: true
    readonly property color shadowColor: "#80000000"  // 50% opacity black
    readonly property real shadowBlur: 0.5
    readonly property int shadowVerticalOffset: 10

    // =========================================================================
    // ANIMATIONS - Duration and easing
    // =========================================================================

    readonly property int animationDuration: 200      // Standard animation
    readonly property int animationDurationFast: 150  // Fast feedback
    readonly property int animationDurationSlow: 500  // Slow/blink effects

    // =========================================================================
    // HELPER FUNCTIONS
    // =========================================================================

    // Generate content panel background color from accent
    function contentPanelBg(accent: color): color {
        return Qt.rgba(accent.r, accent.g, accent.b, contentPanelBgOpacity)
    }

    // Generate content panel border color from accent
    function contentPanelBorder(accent: color): color {
        return Qt.rgba(accent.r, accent.g, accent.b, contentPanelBorderOpacity)
    }

    // Generate value box background color from accent
    function valueBoxBg(accent: color): color {
        return Qt.rgba(accent.r, accent.g, accent.b, 0.1)
    }

    // Generate value box border color from accent
    function valueBoxBorder(accent: color): color {
        return Qt.rgba(accent.r, accent.g, accent.b, 0.2)
    }

    // Generate progress bar track color from accent
    function progressTrackColor(accent: color): color {
        return Qt.rgba(accent.r, accent.g, accent.b, 0.15)
    }
}
