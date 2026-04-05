import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 960
    height: 720
    visible: true
    title: qsTr("RSSI Bomb — client")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Label { text: qsTr("Host") }
            TextField {
                id: hostField
                Layout.fillWidth: true
                text: game.host
                onEditingFinished: game.host = text
            }
            Label { text: qsTr("Port") }
            SpinBox {
                id: portSpin
                from: 1
                to: 65535
                value: game.port
                onValueChanged: game.port = value
            }
            Button {
                text: qsTr("Connect")
                onClicked: {
                    game.host = hostField.text
                    game.port = portSpin.value
                    game.connectToServer()
                }
            }
            Button {
                text: qsTr("Disconnect")
                onClicked: game.disconnectFromServer()
            }
        }

        RowLayout {
            Button {
                text: qsTr("Create lobby")
                onClicked: game.createLobby()
            }
            Label { text: qsTr("Invite") }
            TextField {
                id: inviteField
                Layout.fillWidth: true
                text: game.inviteCode
                readOnly: true
            }
            Label { text: qsTr("Join code") }
            TextField {
                id: joinField
                Layout.preferredWidth: 120
            }
            Button {
                text: qsTr("Join")
                onClicked: game.joinLobby(joinField.text)
            }
        }

        Label {
            text: qsTr("Phase: ") + game.phase + " — " + game.status
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Grid {
                id: fieldGrid
                columns: game.gridW
                rowSpacing: 2
                columnSpacing: 2
                Layout.fillWidth: true
                Layout.fillHeight: true

                Repeater {
                    model: game.gridW * game.gridH
                    Rectangle {
                        width: Math.max(4, (fieldGrid.width - fieldGrid.columnSpacing * (game.gridW - 1)) / game.gridW)
                        height: Math.max(4, (fieldGrid.height - fieldGrid.rowSpacing * (game.gridH - 1)) / game.gridH)
                        border.color: "#888"
                        color: index < game.cellColors.length ? game.cellColors[index] : "#eeeeee"
                    }
                }
            }

            ColumnLayout {
                Label { text: qsTr("Hider: place TX"); font.bold: true }
                Button {
                    text: qsTr("PLACE 1,1 2,2 3,3")
                    onClicked: game.placeTransmitters(1, 1, 2, 2, 3, 3)
                }
                Label { text: qsTr("Seeker"); font.bold: true }
                Button {
                    text: qsTr("GRADIENT_STEP")
                    onClicked: game.sendGradientStep()
                }
                Button {
                    text: qsTr("DONE")
                    onClicked: game.sendDone()
                }
                Label { text: qsTr("Last STATE"); font.bold: true }
                ScrollView {
                    Layout.preferredWidth: 360
                    Layout.preferredHeight: 200
                    TextArea {
                        readOnly: true
                        wrapMode: Text.Wrap
                        text: game.lastStateLine
                    }
                }
            }
        }
    }
}
