import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Configuration 1.0
import harbour.nextcloudnotes.notesmodel 1.0
import harbour.nextcloudnotes.sslconfiguration 1.0
import "pages"
import "components"

ApplicationWindow
{
    id: appWindow

    ConfigurationValue {
        id: accounts
        key: appSettings.path + "/accountIDs"
        defaultValue: [ ]
    }

    ConfigurationGroup {
        id: account
        path: "/apps/harbour-nextcloudnotes/accounts/" + appSettings.currentAccount
        property string name: value("name", "", String)
        property url server: value("server", "", String)
        property string version: value("version", "v0.2", String)
        property string username: value("username", "", String)
        property string password: account.value("password", "", String)
        property bool unsecureConnection: account.value("unsecureConnection", false, Boolean)
        property bool unencryptedConnection: account.value("unencryptedConnection", false, Boolean)
        property date update: value("update", "", Date)
        onValuesChanged: console.log("A property of the current account has changed")
    }

    ConfigurationGroup {
        id: appSettings
        path: "/apps/harbour-nextcloudnotes/settings"

        property string currentAccount: value("currentAccount", "")
        property var accountIDs: value("accountIDs", [ ])
        property int autoSyncInterval: value("autoSyncInterval", 0)
        property int previewLineCount: value("previewLineCount", 4)
        property bool favoritesOnTop: value("favoritesOnTop", true, Boolean)
        property string sortBy: value("sortBy", "date", String)
        property bool showSeparator: value("showSeparator", false, Boolean)
        property bool useMonoFont: value("useMonoFont", false, Boolean)
        property bool useCapitalX: value("useCapitalX", false, Boolean)
        onCurrentAccountChanged: api.uuid = currentAccount

        function addAccount() {
            var uuid = uuidv4()
            var tmpIDs = accounts.value
            tmpIDs.push(uuid)
            accounts.value = tmpIDs
            accounts.sync()
            return uuid
        }
        function removeAccount(uuid) {
            autoSyncTimer.stop()
            var tmpAccouunt = currentAccount
            currentAccount = uuid
            api.clear()
            currentAccount = tmpAccouunt
            var newIds = [ ]
            accountIDs.forEach(function(currentValue) {
                if (currentValue !== uuid) {
                    newIds.push(currentValue)
                }
            })
            accounts.value = newIds
            for (var i = accountIDs.length-1; i > 0; i--) {
                if (accountIDs[i] !== uuid) {
                    api.uuid = accountIDs[i]
                    break
                }
            }
            autoSyncTimer.start()
        }
        function uuidv4() {
            return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
                var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8);
                return v.toString(16);
            });
        }
    }

    SslConfiguration {
        id: ssl
        checkCert: !account.unsecureConnection
    }

    Timer {
        id: autoSyncTimer
        interval: appSettings.autoSyncInterval * 1000
        repeat: true
        running: interval > 0 && appWindow.visible
        triggeredOnStart: true
        onTriggered: {
            if (!api.busy) {
                api.getNotesFromApi()
            }
            else {
                triggeredOnStart = false
                restart()
                triggeredOnStart = true
            }
        }
        onIntervalChanged: console.log("Auto-Sync every " + interval / 1000 + " seconds")
    }

    NotesApi {
        id: api
        uuid: appSettings.currentAccount
        onResponseChanged: noteListModel.applyJSON(response)
        onUuidChanged: noteListModel.clear()
    }

    NotesModel {
        id: noteListModel
        sortBy: 0 // TODO
        favoritesOnTop: appSettings.favoritesOnTop
    }

    /*NoteDelegateModel {
        id: noteListModel
        model: api.model
        favoritesOnTop: appSettings.favoritesOnTop
        sortBy: appSettings.sortBy
        showSeparator: appSettings.showSeparator
        previewLineCount: appSettings.previewLineCount
    }*/

    initialPage: Component { NotesPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}
