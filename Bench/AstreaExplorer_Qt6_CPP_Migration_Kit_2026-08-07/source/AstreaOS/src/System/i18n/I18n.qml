pragma Singleton

import QtQuick

QtObject {
    id: root

    // qmllint disable missing-property
    function nativeValue(name, fallback) {
        var application = Qt.application
        if (!application || !application.property)
            return fallback
        var value = application.property(name)
        return value === undefined || value === null ? fallback : value
    }
    // qmllint enable missing-property

    // Catalogs are loaded by the native Explorer bootstrap. QML only exposes
    // the immutable presentation data to existing consumers.
    readonly property string astreaRoot: root.nativeValue("astreaRuntimeRoot", "")
    readonly property string helperPath: ""
    readonly property string configPath: ""

    property string language: root.nativeValue("astreaI18nLanguage", "en_US")
    property var messages: root.nativeValue("astreaI18nMessages", ({}))
    property var strings: root.nativeValue("astreaI18nStrings", ({}))
    property var fallbackStrings: root.nativeValue("astreaI18nFallbackStrings", ({}))
    property bool ready: true


    function tr(key, fallback, params) {
        var value = (messages && messages[key]) || fallback || key
        if (params) {
            for (var name in params)
                value = value.replace(new RegExp("\\{" + name + "\\}", "g"), String(params[name]))
        }
        return value
    }

    function reload() {
        language = root.nativeValue("astreaI18nLanguage", "en_US")
        messages = root.nativeValue("astreaI18nMessages", ({}))
        strings = root.nativeValue("astreaI18nStrings", ({}))
        fallbackStrings = root.nativeValue("astreaI18nFallbackStrings", ({}))
        ready = true
    }

    Component.onCompleted: reload()
}
