#include "notesapi.h"
#include <QGuiApplication>
#include <QAuthenticator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

NotesApi::NotesApi(const QString statusEndpoint, const QString loginEndpoint, const QString ocsEndpoint, const QString notesEndpoint, QObject *parent)
    : m_statusEndpoint(statusEndpoint), m_loginEndpoint(loginEndpoint), m_ocsEndpoint(ocsEndpoint), m_notesEndpoint(notesEndpoint)
{
    // TODO verify connections (also in destructor)
    m_loginPollTimer.setInterval(POLL_INTERVALL);
    connect(&m_loginPollTimer, SIGNAL(timeout()), this, SLOT(pollLoginUrl()));
    setNcStatusStatus(NextcloudStatus::NextcloudUnknown);
    setLoginStatus(LoginStatus::LoginUnknown);
    m_ncStatusStatus = NextcloudStatus::NextcloudUnknown;
    m_status_installed = false;
    m_status_maintenance = false;
    m_status_needsDbUpgrade = false;
    m_status_extendedSupport = false;
    m_loginStatus = LoginStatus::LoginUnknown;
    connect(this, SIGNAL(urlChanged(QUrl)), this, SLOT(verifyUrl(QUrl)));
    connect(&m_manager, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this, SLOT(requireAuthentication(QNetworkReply*,QAuthenticator*)));
    connect(&m_manager, SIGNAL(networkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility)), this, SLOT(onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility)));
    connect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
    connect(&m_manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(sslError(QNetworkReply*,QList<QSslError>)));
    m_request.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    m_request.setHeader(QNetworkRequest::UserAgentHeader, QGuiApplication::applicationDisplayName() +  " " + QGuiApplication::applicationVersion() + " - " + QSysInfo::machineHostName());
    m_request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/x-www-form-urlencoded").toUtf8());
    m_request.setRawHeader("OCS-APIREQUEST", "true");
    m_authenticatedRequest = m_request;
    m_authenticatedRequest.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json").toUtf8());
}

NotesApi::~NotesApi() {
    disconnect(&m_loginPollTimer, SIGNAL(timeout()), this, SLOT(pollLoginUrl()));
    disconnect(this, SIGNAL(urlChanged(QUrl)), this, SLOT(verifyUrl(QUrl)));
    disconnect(&m_manager, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this, SLOT(requireAuthentication(QNetworkReply*,QAuthenticator*)));
    disconnect(&m_manager, SIGNAL(networkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility)), this, SLOT(onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility)));
    disconnect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
    disconnect(&m_manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(sslError(QNetworkReply*,QList<QSslError>)));
}

const QList<int> NotesApi::noteIds() {
    return m_syncedNotes.keys();
}

bool NotesApi::noteExists(const int id) {
    return m_syncedNotes.contains(id);
}

int NotesApi::noteModified(const int id) {
    return m_syncedNotes.value(id, -1);
}

bool NotesApi::getAllNotes(const QStringList& exclude) {
    qDebug() << "Getting all notes";
    QUrl url = apiEndpointUrl(m_notesEndpoint);

    if (!exclude.isEmpty())
        url.setQuery(QString(EXCLUDE_QUERY).append(exclude.join(",")));

    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        qDebug() << "GET" << url.toDisplayString();
        m_authenticatedRequest.setUrl(url);
        m_getAllNotesReplies << m_manager.get(m_authenticatedRequest);
        emit busyChanged(true);
        return true;
    }
    return false;
}

bool NotesApi::getNote(const int id, const QStringList& exclude) {
    qDebug() << "Getting note: " << id;
    QUrl url = apiEndpointUrl(m_notesEndpoint + QString("/%1").arg(id));
    if (!exclude.isEmpty())
        url.setQuery(QString(EXCLUDE_QUERY).append(exclude.join(",")));

    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        qDebug() << "GET" << url.toDisplayString();
        m_authenticatedRequest.setUrl(url);
        m_getNoteReplies << m_manager.get(m_authenticatedRequest);
        emit busyChanged(true);
        return true;
    }
    return false;
}

bool NotesApi::createNote(const QJsonObject& note) {
    qDebug() << "Creating note";
    QUrl url = apiEndpointUrl(m_notesEndpoint);
    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        qDebug() << "POST" << url.toDisplayString();
        m_authenticatedRequest.setUrl(url);
        m_createNoteReplies << m_manager.post(m_authenticatedRequest, QJsonDocument(note).toJson());
        emit busyChanged(true);
        return true;
    }
    return false;
}

bool NotesApi::updateNote(const int id, const QJsonObject& note) {
    qDebug() << "Updating note: " << id;
    QUrl url = apiEndpointUrl(m_notesEndpoint + QString("/%1").arg(id));
    if (id >= 0 && url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        qDebug() << "PUT" << url.toDisplayString();
        m_authenticatedRequest.setUrl(url);
        m_updateNoteReplies << m_manager.put(m_authenticatedRequest, QJsonDocument(note).toJson());
        emit busyChanged(true);
        return true;
    }
    return false;
}

bool NotesApi::deleteNote(const int id) {
    qDebug() << "Deleting note: " << id;
    QUrl url = apiEndpointUrl(m_notesEndpoint + QString("/%1").arg(id));
    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        qDebug() << "DELETE" << url.toDisplayString();
        m_authenticatedRequest.setUrl(url);
        m_deleteNoteReplies << m_manager.deleteResource(m_authenticatedRequest);
        emit busyChanged(true);
        return true;
    }
    return false;
}

bool NotesApi::busy() const {
    return !(m_getAllNotesReplies.empty() &&
             m_getNoteReplies.empty() &&
             m_createNoteReplies.empty() &&
             m_updateNoteReplies.empty() &&
             m_deleteNoteReplies.empty() &&
             m_statusReplies.empty() &&
             m_loginReplies.empty() &&
             m_pollReplies.empty() &&
             m_ocsReplies.empty());
}

void NotesApi::setVerifySsl(bool verify) {
    if (verify != (m_request.sslConfiguration().peerVerifyMode() == QSslSocket::VerifyPeer)) {
        m_request.sslConfiguration().setPeerVerifyMode(verify ? QSslSocket::VerifyPeer : QSslSocket::VerifyNone);
        emit verifySslChanged(verify);
    }
    if (verify != (m_authenticatedRequest.sslConfiguration().peerVerifyMode() == QSslSocket::VerifyPeer)) {
        m_authenticatedRequest.sslConfiguration().setPeerVerifyMode(verify ? QSslSocket::VerifyPeer : QSslSocket::VerifyNone);
        emit verifySslChanged(verify);
    }
}

void NotesApi::setUrl(QUrl url) {
    if (url != m_url) {
        QString oldServer = server();
        setScheme(url.scheme());
        setHost(url.host());
        setPort(url.port());
        setUsername(url.userName());
        setPassword(url.password());
        setPath(url.path());
        emit urlChanged(m_url);
        if (server() != oldServer)
            emit serverChanged(server());
    }
}

QString NotesApi::server() const {
    QUrl server;
    server.setScheme(m_url.scheme());
    server.setHost(m_url.host());
    if (m_url.port() > 0)
        server.setPort(m_url.port());
    server.setPath(m_url.path());
    return server.toString();
}

void NotesApi::setServer(QString serverUrl) {
    QUrl url(serverUrl.trimmed());
    if (url != server()) {
        setScheme(url.scheme());
        setHost(url.host());
        setPort(url.port());
        setPath(url.path());
    }
}

void NotesApi::setScheme(QString scheme) {
    if (scheme != m_url.scheme() && (scheme == "http" || scheme == "https")) {
        m_url.setScheme(scheme);
        emit schemeChanged(m_url.scheme());
        emit serverChanged(server());
        emit urlChanged(m_url);
    }
}

void NotesApi::setHost(QString host) {
    if (host != m_url.host()) {
        m_url.setHost(host);
        emit hostChanged(m_url.host());
        emit serverChanged(server());
        emit urlChanged(m_url);
    }
}

void NotesApi::setPort(int port) {
    if (port != m_url.port() && port >= 1 && port <= 65535) {
        m_url.setPort(port);
        emit portChanged(m_url.port());
        emit serverChanged(server());
        emit urlChanged(m_url);
    }
}

void NotesApi::setUsername(QString username) {
    if (username != m_url.userName()) {
        m_url.setUserName(username);
        QString concatenated = username + ":" + password();
        QByteArray data = concatenated.toLocal8Bit().toBase64();
        QString headerData = "Basic " + data;
        m_authenticatedRequest.setRawHeader("Authorization", headerData.toLocal8Bit());
        emit usernameChanged(m_url.userName());
        emit urlChanged(m_url);
    }
}

void NotesApi::setPassword(QString password) {
    if (password != m_url.password()) {
        m_url.setPassword(password);
        QString concatenated = username() + ":" + password;
        QByteArray data = concatenated.toLocal8Bit().toBase64();
        QString headerData = "Basic " + data;
        m_authenticatedRequest.setRawHeader("Authorization", headerData.toLocal8Bit());
        emit passwordChanged(m_url.password());
        emit urlChanged(m_url);
    }
}

void NotesApi::setPath(QString path) {
    if (path != m_url.path()) {
        m_url.setPath(path);
        emit pathChanged(m_url.path());
        emit serverChanged(server());
        emit urlChanged(m_url);
    }
}

bool NotesApi::getNcStatus() {
    QUrl url = apiEndpointUrl(m_statusEndpoint);
    qDebug() << "GET" << url.toDisplayString();
    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        setNcStatusStatus(NextcloudStatus::NextcloudBusy);
        m_request.setUrl(url);
        m_statusReplies << m_manager.get(m_request);
        return true;
    }
    else {
        qDebug() << "URL not valid!";
        setNcStatusStatus(NextcloudStatus::NextcloudUnknown);
    }
    return false;
}

bool NotesApi::initiateFlowV2Login() {
    if (m_loginStatus == LoginStatus::LoginFlowV2Initiating || m_loginStatus == LoginStatus::LoginFlowV2Polling) {
        abortFlowV2Login();
    }
    QUrl url = apiEndpointUrl(m_loginEndpoint);
    qDebug() << "POST" << url.toDisplayString();
    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        setLoginStatus(LoginStatus::LoginFlowV2Initiating);
        m_request.setUrl(url);
        m_loginReplies << m_manager.post(m_request, QByteArray());
        return true;
    }
    else {
        qDebug() << "URL not valid!";
        setLoginStatus(LoginStatus::LoginFlowV2Failed);
        abortFlowV2Login();
    }
    return false;
}

void NotesApi::abortFlowV2Login() {
    m_loginPollTimer.stop();
    m_loginUrl.clear();
    emit loginUrlChanged(m_loginUrl);
    m_pollUrl.clear();
    m_pollToken.clear();
    setLoginStatus(LoginStatus::LoginUnknown);
}

void NotesApi::pollLoginUrl() {
    qDebug() << "POST" << m_pollUrl.toDisplayString();
    if (m_pollUrl.isValid() && !m_pollUrl.scheme().isEmpty() && !m_pollUrl.host().isEmpty() && !m_pollToken.isEmpty()) {
        m_request.setUrl(m_pollUrl);
        m_pollReplies << m_manager.post(m_request, QByteArray("token=").append(m_pollToken));
    }
    else {
        qDebug() << "URL not valid!";
        setLoginStatus(LoginStatus::LoginFlowV2Failed);
        abortFlowV2Login();
    }
}

void NotesApi::verifyLogin(QString username, QString password) {
    m_ocsRequest = m_authenticatedRequest;
    if (username.isEmpty())
        username = this->username();
    if (password.isEmpty())
        password = this->password();
    QUrl url = apiEndpointUrl(m_ocsEndpoint + QString("/users/%1").arg(username));
    m_ocsRequest.setRawHeader("Authorization", "Basic " + QString(username + ":" + password).toLocal8Bit().toBase64());
    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        qDebug() << "GET" << url.toDisplayString();
        m_ocsRequest.setUrl(url);
        m_ocsReplies << m_manager.get(m_ocsRequest);
        emit busyChanged(true);
    }
}

const QString NotesApi::errorMessage(int error) const {
    QString message;
    switch (error) {
    case NoError:
        message = tr("No error");
        break;
    case NoConnectionError:
        message = tr("No network connection available");
        break;
    case CommunicationError:
        message = tr("Failed to communicate with the Nextcloud server");
        break;
    case SslHandshakeError:
        message = tr("An error occured while establishing an encrypted connection");
        break;
    case AuthenticationError:
        message = tr("Could not authenticate to the Nextcloud instance");
        break;
    default:
        message = tr("Unknown error");
        break;
    }
    return message;
}

void NotesApi::verifyUrl(QUrl url) {
    emit urlValidChanged(url.isValid());
}

void NotesApi::requireAuthentication(QNetworkReply *reply, QAuthenticator *authenticator) {
    if (reply && authenticator) {
        authenticator->setUser(username());
        authenticator->setPassword(password());
    }
    else
        emit noteError(AuthenticationError);
}

void NotesApi::onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible) {
    emit networkAccessibleChanged(accessible == QNetworkAccessManager::Accessible);
}

void NotesApi::replyFinished(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError)
        qDebug() << reply->error() << reply->errorString();

    if (reply->error() == QNetworkReply::NoError)
        emit noteError(NoError);
    else if (reply->error() == QNetworkReply::AuthenticationRequiredError)
        emit noteError(AuthenticationError);
    else if (reply->error() == QNetworkReply::ContentNotFoundError && m_pollReplies.contains(reply))
        emit noteError(NoError);
    else
        emit noteError(CommunicationError);

    QByteArray data = reply->readAll();
    QJsonDocument json = QJsonDocument::fromJson(data);

    if (m_getAllNotesReplies.contains(reply)) {
        qDebug() << "Get all notes reply";
        if (reply->error() == QNetworkReply::NoError && json.isArray()) {
            updateApiNotes(json.array());
        }
        m_getAllNotesReplies.removeOne(reply);
    }
    else if (m_getNoteReplies.contains(reply)) {
        qDebug() << "Get note reply";
        if (reply->error() == QNetworkReply::NoError && json.isObject())
            updateApiNote(json.object());
        m_getNoteReplies.removeOne(reply);
    }
    else if (m_createNoteReplies.contains(reply)) {
        qDebug() << "Create note reply";
       if (reply->error() == QNetworkReply::NoError && json.isObject())
            createApiNote(json.object());
        m_createNoteReplies.removeOne(reply);
    }
    else if (m_updateNoteReplies.contains(reply)) {
        qDebug() << "Update note reply";bool ok;
        QString idString = reply->url().path().split('/', QString::SkipEmptyParts).last();
        int id = idString.toInt(&ok);
        if (reply->error() == QNetworkReply::NoError && json.isObject() && id >= 0 && ok) {
            QJsonObject obj = json.object();
            obj["id"] = id;
            updateApiNote(obj);
        }
        m_updateNoteReplies.removeOne(reply);
    }
    else if (m_deleteNoteReplies.contains(reply)) {
        qDebug() << "Delete note reply";
        bool ok;
        QString idString = reply->url().path().split('/', QString::SkipEmptyParts).last();
        int id = idString.toInt(&ok);
        if (reply->error() == QNetworkReply::NoError && id >= 0 && ok) {
            m_syncedNotes.remove(id);
            emit noteDeleted(id);
        }
        m_deleteNoteReplies.removeOne(reply);
    }
    else if (m_loginReplies.contains(reply)) {
        qDebug() << "Login reply";
        if (reply->error() == QNetworkReply::NoError && json.isObject())
            updateLoginFlow(json.object());
        else {
            m_loginStatus = LoginStatus::LoginFailed;
            emit loginStatusChanged(m_loginStatus);
        }
        m_loginReplies.removeOne(reply);
    }
    else if (m_pollReplies.contains(reply)) {
        qDebug() << "Poll reply, finished";
        if (reply->error() == QNetworkReply::NoError && json.isObject())
            updateLoginCredentials(json.object());
        else if (reply->error() == QNetworkReply::ContentNotFoundError) {
            qDebug() << "Polling not finished yet" << reply->url().toDisplayString();
            m_loginStatus = LoginStatus::LoginFlowV2Polling;
            emit loginStatusChanged(m_loginStatus);
        }
        else {
            m_loginStatus = LoginStatus::LoginFailed;
            emit loginStatusChanged(m_loginStatus);
        }
        m_pollReplies.removeOne(reply);
        abortFlowV2Login();
    }
    else if (m_statusReplies.contains(reply)) {
        qDebug() << "Status reply";
        if (reply->error() == QNetworkReply::NoError && json.isObject())
            updateNcStatus(json.object());
        else
            updateNcStatus(QJsonObject());
        m_statusReplies.removeOne(reply);
    }
    else if (m_ocsReplies.contains(reply)) {
        qDebug() << "OCS reply";
        QString xml(data);
        if (reply->error() == QNetworkReply::NoError && xml.contains("<status>ok</status>")) {
            qDebug() << "Login Success!";
            setLoginStatus(LoginSuccess);
        }
        else {
            qDebug() << "Login Failed!";
            setLoginStatus(LoginFailed);
        }
        m_ocsReplies.removeOne(reply);
    }
    else {
        qDebug() << "Unknown reply";
    }
    //qDebug() << data;

    emit busyChanged(busy());
    reply->deleteLater();
}

void NotesApi::sslError(QNetworkReply *reply, const QList<QSslError> &errors) {
    qDebug() << "SSL errors accured while calling" << reply->url().toDisplayString();
    for (int i = 0; i < errors.size(); ++i) {
        qDebug() << errors[i].errorString();
    }
    emit noteError(SslHandshakeError);
}

QUrl NotesApi::apiEndpointUrl(const QString endpoint) const {
    QUrl url = server();
    url.setPath(url.path() + endpoint);
    return url;
}

void NotesApi::updateNcStatus(const QJsonObject &status) {
    if (m_status_installed != status.value("installed").toBool()) {
        m_status_installed = status.value("installed").toBool();
        emit statusInstalledChanged(m_status_installed);
    }
    if (m_status_maintenance != status.value("maintenance").toBool()) {
        m_status_maintenance = status.value("maintenance").toBool();
        emit statusMaintenanceChanged(m_status_maintenance);
    }
    if (m_status_needsDbUpgrade != status.value("needsDbUpgrade").toBool()) {
        m_status_needsDbUpgrade = status.value("needsDbUpgrade").toBool();
        emit statusNeedsDbUpgradeChanged(m_status_needsDbUpgrade);
    }
    if (m_status_version != status.value("version").toString()) {
        m_status_version = status.value("version").toString();
        emit statusVersionChanged(m_status_version);
    }
    if (m_status_versionstring != status.value("versionstring").toString()) {
        m_status_versionstring = status.value("versionstring").toString();
        emit statusVersionStringChanged(m_status_versionstring);
    }
    if (m_status_edition != status.value("edition").toString()) {
        m_status_edition = status.value("edition").toString();
        emit statusEditionChanged(m_status_edition);
    }
    if (m_status_productname != status.value("productname").toString()) {
        m_status_productname = status.value("productname").toString();
        emit statusProductNameChanged(m_status_productname);
    }
    if (m_status_extendedSupport != status.value("extendedSupport").toBool()) {
        m_status_extendedSupport = status.value("extendedSupport").toBool();
        emit statusExtendedSupportChanged(m_status_extendedSupport);
    }
    if (status.isEmpty())
        setNcStatusStatus(NextcloudStatus::NextcloudFailed);
    else
        setNcStatusStatus(NextcloudStatus::NextcloudSuccess);
}

void NotesApi::setNcStatusStatus(NextcloudStatus status, bool *changed) {
    if (status != m_ncStatusStatus) {
        if (changed)
            *changed = true;
        m_ncStatusStatus = status;
        emit ncStatusStatusChanged(m_ncStatusStatus);
    }
}

bool NotesApi::updateLoginFlow(const QJsonObject &login) {
    QUrl url;
    QString token;
    if (!login.isEmpty()) {
        QJsonObject poll = login.value("poll").toObject();
        url = login.value("login").toString();
        if (!poll.isEmpty() && url.isValid()) {
            if (url != m_loginUrl) {
                m_loginUrl = url;
                emit loginUrlChanged(m_loginUrl);
            }
            url = poll.value("endpoint").toString();
            token = poll.value("token").toString();
            if (url.isValid() && !token.isEmpty()) {
                m_pollUrl = url;
                qDebug() << "Poll URL: " << m_pollUrl;
                m_pollToken = token;
                qDebug() << "Poll Token: " << m_pollToken;
                setLoginStatus(LoginStatus::LoginFlowV2Polling);
                m_loginPollTimer.start();
                return true;
            }
        }
    }
    else {
        qDebug() << "Invalid Poll Data:" << login;
        setLoginStatus(LoginStatus::LoginFlowV2Failed);
        abortFlowV2Login();
    }
    return false;
}

bool NotesApi::updateLoginCredentials(const QJsonObject &credentials) {
    QString serverAddr;
    QString loginName;
    QString appPassword;
    if (!credentials.isEmpty()) {
        serverAddr = credentials.value("server").toString();
        if (!serverAddr.isEmpty() && serverAddr != server())
            setServer(serverAddr);
        loginName = credentials.value("loginName").toString();
        if (!loginName.isEmpty() && loginName != username())
            setUsername(loginName);
        appPassword = credentials.value("appPassword").toString();
        if (!appPassword.isEmpty() && appPassword != password())
            setPassword(appPassword);
    }
    if (!serverAddr.isEmpty() && !loginName.isEmpty() && !appPassword.isEmpty()) {
        qDebug() << "Login successfull for user" << loginName << "on" << serverAddr;
        setLoginStatus(LoginStatus::LoginFlowV2Success);
        return true;
    }
    qDebug() << "Login failed for user" << loginName << "on" << serverAddr;
    return false;
}

void NotesApi::setLoginStatus(LoginStatus status, bool *changed) {
    if (status != m_loginStatus) {
        if (changed)
            *changed = true;
        m_loginStatus = status;
        emit loginStatusChanged(m_loginStatus);
    }
}

void NotesApi::updateApiNotes(const QJsonArray &json) {
    for (int i = 0; i < json.size(); ++i) {
        if (json[i].isObject()) {
            QJsonObject object = json[i].toObject();
            if (!object.isEmpty()) {
                updateApiNote(json[i].toObject());
            }
        }
    }
    m_lastSync = QDateTime::currentDateTime();
    emit lastSyncChanged(m_lastSync);
}

void NotesApi::updateApiNote(const QJsonObject &json) {
    int id = json["id"].toInt(-1);
    if (id >= 0) {
        m_syncedNotes.insert(id, json.value("modified").toInt(-1));
        emit noteUpdated(id, json);
    }
}

void NotesApi::createApiNote(const QJsonObject &json) {
    int id = json["id"].toInt(-1);
    if (id >= 0) {
        m_syncedNotes.insert(id, json.value("modified").toInt(-1));
        emit noteCreated(id, json);
    }
}
