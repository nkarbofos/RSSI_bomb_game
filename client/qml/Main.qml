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
            text: game.myRole.length > 0
                  ? (qsTr("Ваша роль: ") + game.myRole)
                  : qsTr("Роль будет назначена после подключения к игре")
            font.bold: true
        }

        Label {
            text: qsTr("Фаза: ") + game.phase + " — " + game.status
                  + (game.placementSecondsRemaining > 0 && game.phase === "Placement"
                     ? (" — " + qsTr("осталось с: ") + game.placementSecondsRemaining)
                     : "")
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: game.boardVisible
                       && !(game.phase === "Placement" && game.myRole === "Seeker")

                Label {
                    text: qsTr("Поле")
                    font.bold: true
                }

                Grid {
                    id: fieldGrid
                    columns: game.gridW
                    rowSpacing: 2
                    columnSpacing: 2
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Repeater {
                        model: game.gridW * game.gridH
                        Item {
                            width: Math.max(4, (fieldGrid.width - fieldGrid.columnSpacing * (game.gridW - 1)) / game.gridW)
                            height: Math.max(4, (fieldGrid.height - fieldGrid.rowSpacing * (game.gridH - 1)) / game.gridH)

                            Rectangle {
                                anchors.fill: parent
                                border.width: index === game.selectedCellIndex ? 3 : 1
                                border.color: index === game.selectedCellIndex ? "#2980b9" : "#888"
                                color: index < game.cellColors.length ? game.cellColors[index] : "#eeeeee"
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                enabled: (game.phase === "Placement" && game.myRole === "Hider")
                                    || (game.phase === "Search" && game.myRole === "Seeker")
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: {
                                    if (game.phase === "Placement" && game.myRole === "Hider")
                                        game.hiderClickCell(index)
                                    else if (game.phase === "Search" && game.myRole === "Seeker")
                                        game.seekerClickCell(index)
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 380

                Label {
                    visible: game.phase === "Placement" && game.myRole === "Seeker"
                    wrapMode: Text.Wrap
                    width: parent.width
                    text: qsTr("Ожидание: противник расставляет передатчики. Поле откроется после расстановки.")
                }

                ColumnLayout {
                    visible: game.phase === "Placement" && game.myRole === "Hider"
                    spacing: 6
                    Label {
                        text: qsTr("Hider: выберите 3 клетки (клик ЛКМ), затем подтвердите.")
                        font.bold: true
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    Label {
                        text: qsTr("Выбрано: ") + game.placementSelectionCount + " / 3"
                    }
                    Button {
                        text: qsTr("Подтвердить расстановку")
                        enabled: game.placementSelectionCount === 3
                        onClicked: game.confirmPlacementFromSelection()
                    }
                    Button {
                        text: qsTr("Завершить расстановку (случайно / как по таймауту)")
                        onClicked: game.endPlacement()
                    }
                }

                ColumnLayout {
                    visible: game.phase === "Search" && game.myRole === "Seeker"
                    spacing: 6
                    Label {
                        text: qsTr("Seeker: фаза поиска — кликните клетку, затем переместите или добавьте приёмник.")
                        font.bold: true
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    Label {
                        text: game.selectedCellIndex >= 0
                              ? (qsTr("Выбрана клетка: ") + game.selectedCellIndex)
                              : qsTr("Клетка не выбрана.")
                    }
                    RowLayout {
                        Button {
                            text: qsTr("Переместить приёмник 0 сюда")
                            enabled: game.selectedCellIndex >= 0
                            onClicked: game.moveReceiverToSelected(0)
                        }
                    }
                    RowLayout {
                        Button {
                            text: qsTr("Переместить приёмник 1 сюда")
                            enabled: game.selectedCellIndex >= 0
                            onClicked: game.moveReceiverToSelected(1)
                        }
                    }
                    RowLayout {
                        Button {
                            text: qsTr("Переместить приёмник 2 сюда")
                            enabled: game.selectedCellIndex >= 0
                            onClicked: game.moveReceiverToSelected(2)
                        }
                    }
                    Button {
                        text: qsTr("Добавить приёмник на выбранную клетку")
                        enabled: game.selectedCellIndex >= 0
                        onClicked: game.addReceiverAtSelected()
                    }
                    RowLayout {
                        Button {
                            text: qsTr("Удалить приёмник 0")
                            onClicked: game.removeReceiverSlot(0)
                        }
                        Button {
                            text: qsTr("Удалить приёмник 1")
                            onClicked: game.removeReceiverSlot(1)
                        }
                        Button {
                            text: qsTr("Удалить приёмник 2")
                            onClicked: game.removeReceiverSlot(2)
                        }
                    }
                    Button {
                        text: qsTr("GRADIENT_STEP")
                        onClicked: game.sendGradientStep()
                    }
                    Button {
                        text: qsTr("DONE")
                        onClicked: game.sendDone()
                    }
                }

                Label {
                    text: qsTr("Последний STATE")
                    font.bold: true
                }
                ScrollView {
                    Layout.fillWidth: true
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
