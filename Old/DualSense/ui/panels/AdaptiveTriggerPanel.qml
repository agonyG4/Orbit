import QtQuick
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea
import "../controls" as Control

Control.Card {
    id: panel
    property var backend
    property string side: "both"
    property string mode: "feedback"
    property int position: 2
    property int start: 2
    property int stop: 8
    property int strength: 5
    property int snapforce: 5
    property int firstFoot: 4
    property int secondFoot: 7
    property int strengthA: 2
    property int strengthB: 7
    property int amplitude: 5
    property int frequency: 25
    property int period: 20
    property string presetId: "custom"

    function restoreConfig() {
        if (!backend || !backend.config || !backend.config.trigger)
            return
        var saved = backend.config.trigger
        side = saved.side === undefined ? side : saved.side
        mode = saved.mode === undefined ? mode : saved.mode
        position = saved.position === undefined ? position : saved.position
        start = saved.start === undefined ? start : saved.start
        stop = saved.stop === undefined ? stop : saved.stop
        strength = saved.strength === undefined ? strength : saved.strength
        snapforce = saved.snapforce === undefined ? snapforce : saved.snapforce
        firstFoot = saved.first_foot === undefined ? firstFoot : saved.first_foot
        secondFoot = saved.second_foot === undefined ? secondFoot : saved.second_foot
        strengthA = saved.strength_a === undefined ? strengthA : saved.strength_a
        strengthB = saved.strength_b === undefined ? strengthB : saved.strength_b
        amplitude = saved.amplitude === undefined ? amplitude : saved.amplitude
        frequency = saved.frequency === undefined ? frequency : saved.frequency
        period = saved.period === undefined ? period : saved.period
        presetId = saved.preset_id === undefined ? presetId : saved.preset_id
    }

    function setTriggerConfig(triggerConfig, persist) {
        if (!triggerConfig)
            return
        side = triggerConfig.side === undefined ? side : triggerConfig.side
        mode = triggerConfig.mode === undefined ? mode : triggerConfig.mode
        position = triggerConfig.position === undefined ? position : triggerConfig.position
        start = triggerConfig.start === undefined ? start : triggerConfig.start
        stop = triggerConfig.stop === undefined ? stop : triggerConfig.stop
        strength = triggerConfig.strength === undefined ? strength : triggerConfig.strength
        snapforce = triggerConfig.snapforce === undefined ? snapforce : triggerConfig.snapforce
        firstFoot = triggerConfig.first_foot === undefined ? firstFoot : triggerConfig.first_foot
        secondFoot = triggerConfig.second_foot === undefined ? secondFoot : triggerConfig.second_foot
        strengthA = triggerConfig.strength_a === undefined ? strengthA : triggerConfig.strength_a
        strengthB = triggerConfig.strength_b === undefined ? strengthB : triggerConfig.strength_b
        amplitude = triggerConfig.amplitude === undefined ? amplitude : triggerConfig.amplitude
        frequency = triggerConfig.frequency === undefined ? frequency : triggerConfig.frequency
        period = triggerConfig.period === undefined ? period : triggerConfig.period
        presetId = triggerConfig.preset_id === undefined ? "custom" : triggerConfig.preset_id
        if (persist)
            saveConfig()
    }

    function persistCustom() {
        presetId = "custom"
        saveConfig()
    }

    function saveConfig() {
        if (!backend)
            return
        backend.saveConfig({
            "trigger": {
                "side": side,
                "mode": mode,
                "position": position,
                "start": start,
                "stop": stop,
                "strength": strength,
                "snapforce": snapforce,
                "first_foot": firstFoot,
                "second_foot": secondFoot,
                "strength_a": strengthA,
                "strength_b": strengthB,
                "amplitude": amplitude,
                "frequency": frequency,
                "period": period,
                "preset_id": presetId
            }
        })
    }

    function args() {
        var values = ["--trigger-side", side, "--mode", mode]
        if (mode === "feedback")
            return values.concat(["--position", position, "--strength", strength])
        if (mode === "weapon")
            return values.concat(["--start", start, "--stop", stop, "--strength", strength])
        if (mode === "bow")
            return values.concat(["--start", start, "--stop", stop, "--strength", strength, "--snapforce", snapforce])
        if (mode === "galloping")
            return values.concat(["--start", start, "--stop", stop, "--first-foot", firstFoot, "--second-foot", secondFoot, "--frequency", frequency])
        if (mode === "machine")
            return values.concat(["--start", start, "--stop", stop, "--strength-a", strengthA, "--strength-b", strengthB, "--frequency", frequency, "--period", period])
        if (mode === "vibration")
            return values.concat(["--position", position, "--amplitude", amplitude, "--frequency", frequency])
        return values
    }

    function applyText() {
        return panel.backend && panel.backend.ready ? qsTr("Apply live") : qsTr("Saved offline")
    }

    Component.onCompleted: restoreConfig()

    Connections {
        target: panel.backend
        function onConfigLoaded() {
            panel.restoreConfig()
        }
    }

    Astrea.SectionHeader {
        Layout.fillWidth: true
        text: qsTr("Adaptive triggers")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Astrea.SelectButton {
            Layout.preferredWidth: 122
            label: panel.side
            options: ["both", "left", "right"]
            selectedIndex: panel.side === "both" ? 0 : (panel.side === "left" ? 1 : 2)
            onSelected: index => { panel.side = ["both", "left", "right"][index]; panel.persistCustom() }
        }

        Astrea.SelectButton {
            Layout.preferredWidth: 164
            label: panel.mode
            options: ["off", "feedback", "weapon", "bow", "galloping", "machine", "vibration"]
            selectedIndex: ["off", "feedback", "weapon", "bow", "galloping", "machine", "vibration"].indexOf(panel.mode)
            onSelected: index => { panel.mode = ["off", "feedback", "weapon", "bow", "galloping", "machine", "vibration"][index]; panel.persistCustom() }
        }

        Item { Layout.fillWidth: true }

        Control.PrimaryButton {
            text: panel.applyText()
            primary: panel.backend && panel.backend.ready
            flat: !(panel.backend && panel.backend.ready)
            enabledState: panel.backend && panel.backend.ready && !panel.backend.applying
            onClicked: panel.backend.applyAction("trigger", panel.args(), false)
        }
    }

    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "feedback" || panel.mode === "vibration"; label: qsTr("Position"); from: 0; to: 9; value: panel.position; onEdited: value => { panel.position = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "weapon" || panel.mode === "bow" || panel.mode === "galloping" || panel.mode === "machine"; label: qsTr("Start"); from: 0; to: 9; value: panel.start; onEdited: value => { panel.start = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "weapon" || panel.mode === "bow" || panel.mode === "galloping" || panel.mode === "machine"; label: qsTr("Stop"); from: 0; to: 9; value: panel.stop; onEdited: value => { panel.stop = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "feedback" || panel.mode === "weapon" || panel.mode === "bow"; label: qsTr("Strength"); from: 0; to: 8; value: panel.strength; onEdited: value => { panel.strength = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "bow"; label: qsTr("Snapforce"); from: 0; to: 8; value: panel.snapforce; onEdited: value => { panel.snapforce = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "galloping"; label: qsTr("First foot"); from: 0; to: 8; value: panel.firstFoot; onEdited: value => { panel.firstFoot = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "galloping"; label: qsTr("Second foot"); from: 0; to: 8; value: panel.secondFoot; onEdited: value => { panel.secondFoot = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "machine"; label: qsTr("Strength A"); from: 0; to: 8; value: panel.strengthA; onEdited: value => { panel.strengthA = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "machine"; label: qsTr("Strength B"); from: 0; to: 8; value: panel.strengthB; onEdited: value => { panel.strengthB = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "vibration"; label: qsTr("Amplitude"); from: 0; to: 8; value: panel.amplitude; onEdited: value => { panel.amplitude = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "galloping" || panel.mode === "machine" || panel.mode === "vibration"; label: qsTr("Frequency"); from: 0; to: 255; value: panel.frequency; onEdited: value => { panel.frequency = value; panel.persistCustom() } }
    Control.ValueSlider { Layout.fillWidth: true; visible: panel.mode === "machine"; label: qsTr("Period"); from: 0; to: 255; value: panel.period; onEdited: value => { panel.period = value; panel.persistCustom() } }
}
