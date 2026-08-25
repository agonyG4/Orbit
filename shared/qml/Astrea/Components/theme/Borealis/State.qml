pragma Singleton
import QtQuick

Item {
    id: state
    visible: false
    width: 0
    height: 0

    // Theme persistence is owned by the native application layer. These
    // compatibility properties remain for the shared QML theme facade.
    readonly property string configPath: ""
    readonly property string colorSchemeApplyPath: ""
    readonly property string decorationApplyPath: ""
    property bool loaded: true

    property int themeMode: 0
    property int shellStyle: 0
    property int iconStyle: 0
    property string iconTheme: "dark"
    property string accentHex: "#0a84ff"
    property int audioOsdStyle: 0
    property int persistedShellStyle: 0

    function applyConfig(cfg) {
        if (!cfg)
            return

        var nextThemeMode = cfg.theme_mode === 1 ? 1 : 0
        if (typeof cfg.theme === "string")
            nextThemeMode = cfg.theme.toLowerCase() === "light" ? 1 : 0
        state.themeMode = nextThemeMode

        var nextShellStyle = typeof cfg.shell_style === "number" ? cfg.shell_style : 1
        if (nextShellStyle < 0 || nextShellStyle > 2)
            nextShellStyle = 1
        state.shellStyle = nextShellStyle
        state.persistedShellStyle = nextShellStyle

        state.iconStyle = typeof cfg.icon_style === "number" ? cfg.icon_style : 0
        state.iconTheme = typeof cfg.icon_theme === "string" ? cfg.icon_theme : "dark"
        state.accentHex = typeof cfg.accent === "string" && cfg.accent !== ""
            ? cfg.accent
            : "#0a84ff"

        var nextAudioOsdStyle = typeof cfg.audio_osd_style === "number" ? cfg.audio_osd_style : 0
        if (nextAudioOsdStyle < 0 || nextAudioOsdStyle > 1)
            nextAudioOsdStyle = 0
        state.audioOsdStyle = nextAudioOsdStyle
        state.loaded = true
    }

    function save() {
        // The native SettingsService persists theme changes. Keep this
        // callable for existing QML consumers without starting a process.
    }
}
