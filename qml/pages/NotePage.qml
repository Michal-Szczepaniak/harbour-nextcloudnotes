import "../js/showdown-1.9.0/dist/showdown.js" as ShowDown
import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: noteDialog
    property var showdown: ShowDown.showdown
    property var converter: new showdown.Converter(
                                { noHeaderId: true,
                                    simplifiedAutoLink: true,
                                    tables: true,
                                    tasklists: false, // this is handled by the function reloadContent() because LinkedLabel HTML support is to basic
                                    simpleLineBreaks: true,
                                    emoji: true } )

    function reloadContent() {
        modifiedDetail.value = new Date(account.model.get(noteIndex).modified * 1000).toLocaleString(Qt.locale(), Locale.ShortFormat)
        favoriteDetail.value = account.model.get(noteIndex).favorite ? qsTr("yes") : qsTr("no")
        categoryDetail.value = account.model.get(noteIndex).category
        var convertedText = converter.makeHtml(account.model.get(noteIndex).content)
        var occurence = -1
        convertedText = convertedText.replace(/^<li>\[ \]\s(.*)<\/li>$/gm,
                                              function(match, p1, offset) {
                                                  occurence++
                                                  return '<li><a href="tasklist:checkbox_' + occurence + '">☐ ' + p1 + '</a></li>'
                                              } )
        occurence = -1
        convertedText = convertedText.replace(/^<li>\[[xX]\]\s(.*)<\/li>$/gm,
                                              function(match, p1, offset) {
                                                  occurence++
                                                  return '<li><a href="tasklist:uncheckbox_' + occurence + '">☑ ' + p1 + '</a></li>'
                                              } )
        contentLabel.text = convertedText
        //console.log(contentLabel.text)
    }

    acceptDestination: Qt.resolvedUrl("EditPage.qml")
    acceptDestinationProperties: { account: account; noteIndex: noteIndex }
    Component.onCompleted: acceptDestinationProperties = { account: account, noteIndex: noteIndex }
    onStatusChanged: {
        if (status === PageStatus.Active) {
            account.getNote(account.model.get(noteIndex).id)
            reloadContent()
        }
    }
    Connections {
        target: account
        onBusyChanged: {
            if (account.busy === false) {
                reloadContent()
            }
        }
    }

    property var account
    property int noteIndex

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: mainColumn.height

        Column {
            id: mainColumn
            width: parent.width

            RemorsePopup {
                id: remorse
                onTriggered: pageStack.pop()
            }
            PullDownMenu {
                busy: account ? account.busy : false

                MenuItem {
                    text: qsTr("Delete")
                    enabled: account ? true : false
                    //visible: appSettings.currentAccount >= 0
                    onClicked: remorse.execute("Deleting", function() { account.deleteNote(account.model.get(noteIndex).id) } )
                }
                MenuItem {
                    text: enabled ? qsTr("Reload") : qsTr("Updating...")
                    enabled: account ? !account.busy : false
                    //visible: appSettings.currentAccount >= 0
                    onClicked: account.getNote(account.model.get(noteIndex).id)
                }
                MenuLabel {
                    visible: appSettings.currentAccount >= 0
                    text: account ? (
                                        qsTr("Last update") + ": " + (
                                            new Date(account.update).valueOf() !== 0 ?
                                                new Date(account.update).toLocaleString(Qt.locale(), Locale.ShortFormat) :
                                                qsTr("never"))) : ""
                }
            }

            DialogHeader {
                id: dialogHeader
                dialog: noteDialog
                acceptText: qsTr("Edit")
                cancelText: qsTr("Notes")
            }

            Column {
                width: parent.width
                spacing: Theme.paddingLarge

                LinkedLabel {
                    id: contentLabel
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    textFormat: Text.StyledText
                    defaultLinkActions: false
                    onLinkActivated: {
                        var occurence = -1
                        var newContent = account.model.get(noteIndex).content
                        if (/^tasklist:checkbox_(\d+)$/m.test(link)) {
                            newContent = newContent.replace(/^- \[ \]\s(.*)$/gm,
                                                            function(match, p1, offset, string) {
                                                                occurence++
                                                                if (occurence === parseInt(link.split('_')[1])) {
                                                                    return '- [x] ' + p1
                                                                }
                                                                else {
                                                                    return match
                                                                }
                                                            } )
                            account.updateNote(account.model.get(noteIndex).id, { 'content': newContent } )
                        }
                        else if (/^tasklist:uncheckbox_(\d+)$/m.test(link)) {
                            newContent = newContent.replace(/^- \[x\]\s(.*)$/gm,
                                                            function(match, p1, offset, string) {
                                                                occurence++
                                                                if (occurence === parseInt(link.split('_')[1])) {
                                                                    return '- [ ] ' + p1
                                                                }
                                                                else {
                                                                    return match
                                                                }
                                                            } )
                            account.updateNote(account.model.get(noteIndex).id, { 'content': newContent } )
                        }
                        else {
                            Qt.openUrlExternally(link)
                        }
                    }
                }

                Separator {
                    id: separator
                    width: parent.width
                    color: Theme.primaryColor
                    horizontalAlignment: Qt.AlignHCenter
                }

                Column {
                    width: parent.width

                    DetailItem {
                        id: modifiedDetail
                        label: qsTr("Modified")
                    }
                    DetailItem {
                        id: favoriteDetail
                        label: qsTr("Favorite")
                    }
                    DetailItem {
                        id: categoryDetail
                        label: qsTr("Category")
                        visible: value.length > 0
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
