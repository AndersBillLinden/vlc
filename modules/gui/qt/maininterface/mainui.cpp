#include "mainui.hpp"

#include <cassert>

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlqmltypes.hpp"
#include "medialibrary/mlalbummodel.hpp"
#include "medialibrary/mlartistmodel.hpp"
#include "medialibrary/mlalbumtrackmodel.hpp"
#include "medialibrary/mlgenremodel.hpp"
#include "medialibrary/mlurlmodel.hpp"
#include "medialibrary/mlvideomodel.hpp"
#include "medialibrary/mlrecentsmodel.hpp"
#include "medialibrary/mlrecentsvideomodel.hpp"
#include "medialibrary/mlfoldersmodel.hpp"
#include "medialibrary/mlgrouplistmodel.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"
#include "medialibrary/mlplaylistmodel.hpp"
#include "medialibrary/mlplaylist.hpp"

#include "player/player_controller.hpp"
#include "player/player_controlbar_model.hpp"
#include "player/control_list_model.hpp"

#include "dialogs/toolbar/controlbar_profile_model.hpp"
#include "dialogs/toolbar/controlbar_profile.hpp"

#include "playlist/playlist_model.hpp"
#include "playlist/playlist_controller.hpp"

#include "util/item_key_event_filter.hpp"
#include "util/imageluminanceextractor.hpp"
#include "util/i18n.hpp"
#include "util/keyhelper.hpp"
#include "util/systempalette.hpp"
#include "util/sortfilterproxymodel.hpp"
#include "util/navigation_history.hpp"
#include "util/qmlinputitem.hpp"
#include "util/mouse_event_filter.hpp"
#include "util/flickable_scroll_handler.hpp"

#include "dialogs/help/aboutmodel.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"

#include "network/networkmediamodel.hpp"
#include "network/networkdevicemodel.hpp"
#include "network/networksourcesmodel.hpp"
#include "network/servicesdiscoverymodel.hpp"

#include "menus/qml_menu_wrapper.hpp"

#include "widgets/native/roundimage.hpp"
#include "widgets/native/navigation_attached.hpp"

#include "videosurface.hpp"

#include <QScreen>
#include <QQuickWindow>
#include <QQmlContext>
#include <QQmlFileSelector>

using  namespace vlc::playlist;

namespace {

template<class T>
class SingletonRegisterHelper
{
    static QPointer<T> m_instance;

public:
    static QObject* callback(QQmlEngine *engine, QJSEngine *)
    {
        assert(m_instance);
        engine->setObjectOwnership(m_instance, QQmlEngine::ObjectOwnership::CppOwnership);
        return m_instance;
    }

    static void setInstance(T* instance)
    {
        assert(!m_instance);
        m_instance = instance;
    }

    static T* getInstance()
    {
        return m_instance;
    }
};
template<class T>
QPointer<T> SingletonRegisterHelper<T>::m_instance = nullptr;

template<class T>
void registerAnonymousType( const char *uri, int versionMajor )
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    qmlRegisterAnonymousType<T>( uri, versionMajor );
#else
    qmlRegisterType<T>();
    VLC_UNUSED( uri );
    VLC_UNUSED( versionMajor );
#endif
}

} // anonymous namespace


MainUI::MainUI(qt_intf_t *p_intf, MainCtx *mainCtx, QWindow* interfaceWindow,  QObject *parent)
    : QObject(parent)
    , m_intf(p_intf)
    , m_mainCtx(mainCtx)
    , m_interfaceWindow(interfaceWindow)
{
    assert(m_intf);
    assert(m_mainCtx);
    assert(m_interfaceWindow);

    SingletonRegisterHelper<MainCtx>::setInstance(mainCtx);

    assert(m_intf->p_mainPlayerController);
    SingletonRegisterHelper<PlayerController>::setInstance(m_intf->p_mainPlayerController);

    assert(DialogsProvider::getInstance());
    SingletonRegisterHelper<DialogsProvider>::setInstance(DialogsProvider::getInstance());

    SingletonRegisterHelper<NavigationHistory>::setInstance( new NavigationHistory(this) );
    SingletonRegisterHelper<I18n>::setInstance( new I18n(this) );
    SingletonRegisterHelper<SystemPalette>::setInstance( new SystemPalette(this) );
    SingletonRegisterHelper<DialogModel>::setInstance( new DialogModel(m_intf, this));
    SingletonRegisterHelper<QmlKeyHelper>::setInstance( new QmlKeyHelper(this) );

    if (m_mainCtx->hasMediaLibrary())
    {
        assert(m_mainCtx->getMediaLibrary());
        SingletonRegisterHelper<MediaLib>::setInstance(m_mainCtx->getMediaLibrary());
    }

    registerQMLTypes();
}

MainUI::~MainUI()
{

}

bool MainUI::setup(QQmlEngine* engine)
{
    engine->setOutputWarningsToStandardError(false);
    connect(engine, &QQmlEngine::warnings, this, &MainUI::onQmlWarning);

    m_component  = new QQmlComponent(engine, QStringLiteral("qrc:/main/MainInterface.qml"), QQmlComponent::PreferSynchronous, engine);
    if (m_component->isLoading())
    {
        msg_Warn(m_intf, "component is still loading");
    }

    if (m_component->isError())
    {
        for(auto& error: m_component->errors())
            msg_Err(m_intf, "qml loading %s %s:%u", qtu(error.description()), qtu(error.url().toString()), error.line());
#ifdef QT_STATICPLUGIN
            assert( !"Missing qml modules from qt contribs." );
#else
            msg_Err( m_intf, "Install missing modules using your packaging tool" );
#endif
        return false;
    }
    return true;
}

QQuickItem* MainUI::createRootItem()
{
    QObject* rootObject = m_component->create();

    if (m_component->isError())
    {
        for(auto& error: m_component->errors())
            msg_Err(m_intf, "qml loading %s %s:%u", qtu(error.description()), qtu(error.url().toString()), error.line());
        return nullptr;
    }

    if (rootObject == nullptr)
    {
        msg_Err(m_intf, "unable to create main interface, no root item");
        return nullptr;
    }
    m_rootItem = qobject_cast<QQuickItem*>(rootObject);
    if (!m_rootItem)
    {
        msg_Err(m_intf, "unexpected type of qml root item");
        return nullptr;
    }

    return m_rootItem;
}

void MainUI::registerQMLTypes()
{
    {
        const char* uri = "org.videolan.vlc";
        const int versionMajor = 0;
        const int versionMinor = 1;

        qmlRegisterSingletonType<MainCtx>(uri, versionMajor, versionMinor, "MainCtx", SingletonRegisterHelper<MainCtx>::callback);
        qmlRegisterSingletonType<NavigationHistory>(uri, versionMajor, versionMinor, "History", SingletonRegisterHelper<NavigationHistory>::callback);
        qmlRegisterSingletonType<PlayerController>(uri, versionMajor, versionMinor, "Player", SingletonRegisterHelper<PlayerController>::callback);
        qmlRegisterSingletonType<I18n>(uri, versionMajor, versionMinor, "I18n", SingletonRegisterHelper<I18n>::callback);
        qmlRegisterSingletonType<DialogsProvider>(uri, versionMajor, versionMinor, "DialogsProvider", SingletonRegisterHelper<DialogsProvider>::callback);
        qmlRegisterSingletonType<SystemPalette>(uri, versionMajor, versionMinor, "SystemPalette", SingletonRegisterHelper<SystemPalette>::callback);
        qmlRegisterSingletonType<DialogModel>(uri, versionMajor, versionMinor, "DialogModel", SingletonRegisterHelper<DialogModel>::callback);
        qmlRegisterSingletonType<QmlKeyHelper>(uri, versionMajor, versionMinor, "KeyHelper", SingletonRegisterHelper<QmlKeyHelper>::callback);

        qmlRegisterUncreatableType<QAbstractItemModel>(uri, versionMajor, versionMinor, "QtAbstractItemModel", "");
        qmlRegisterUncreatableType<QWindow>(uri, versionMajor, versionMinor, "QtWindow", "");
        qmlRegisterUncreatableType<QScreen>(uri, versionMajor, versionMinor, "QtScreen", "");

        qRegisterMetaType<VLCTick>();
        qmlRegisterUncreatableType<VLCTick>(uri, versionMajor, versionMinor, "VLCTick", "");
        qmlRegisterUncreatableType<ColorSchemeModel>(uri, versionMajor, versionMinor, "ColorSchemeModel", "");

        qRegisterMetaType<QmlInputItem>();

        qmlRegisterType<VideoSurface>(uri, versionMajor, versionMinor, "VideoSurface");

        qRegisterMetaType<NetworkTreeItem>();
        qmlRegisterType<NetworkMediaModel>( uri, versionMajor, versionMinor, "NetworkMediaModel");
        qmlRegisterType<NetworkDeviceModel>( uri, versionMajor, versionMinor, "NetworkDeviceModel");
        qmlRegisterType<NetworkSourcesModel>( uri, versionMajor, versionMinor, "NetworkSourcesModel");
        qmlRegisterType<ServicesDiscoveryModel>( uri, versionMajor, versionMinor, "ServicesDiscoveryModel");
        qmlRegisterType<MLFoldersModel>( uri, versionMajor, versionMinor, "MLFolderModel");
        qmlRegisterType<ImageLuminanceExtractor>( uri, versionMajor, versionMinor, "ImageLuminanceExtractor");

        qmlRegisterUncreatableType<TrackListModel>(uri, versionMajor, versionMinor, "TrackListModel", "available tracks of a media (audio/video/sub)" );
        qmlRegisterUncreatableType<TitleListModel>(uri, versionMajor, versionMinor, "TitleListModel", "available titles of a media" );
        qmlRegisterUncreatableType<ChapterListModel>(uri, versionMajor, versionMinor, "ChapterListModel", "available titles of a media" );
        qmlRegisterUncreatableType<ProgramListModel>(uri, versionMajor, versionMinor, "ProgramListModel", "available programs of a media" );
        qmlRegisterUncreatableType<VLCVarChoiceModel>(uri, versionMajor, versionMinor, "VLCVarChoiceModel", "generic variable with choice model" );

        qRegisterMetaType<PlaylistPtr>();
        qRegisterMetaType<PlaylistItem>();
        qmlRegisterUncreatableType<PlaylistItem>(uri, versionMajor, versionMinor, "PlaylistItem", "");
        qmlRegisterType<PlaylistListModel>( uri, versionMajor, versionMinor, "PlaylistListModel" );
        qmlRegisterType<PlaylistControllerModel>( uri, versionMajor, versionMinor, "PlaylistControllerModel" );

        qmlRegisterType<AboutModel>( uri, versionMajor, versionMinor, "AboutModel" );

        qmlRegisterUncreatableType<DialogErrorModel>( uri, versionMajor, versionMinor, "DialogErrorModel", "");
        qRegisterMetaType<DialogId>();
        qmlRegisterUncreatableType<DialogId>( uri, versionMajor, versionMinor, "DialogId", "");

        qmlRegisterType<ItemKeyEventFilter>( uri, versionMajor, versionMinor, "KeyEventFilter" );
        qmlRegisterType<MouseEventFilter>( uri, versionMajor, versionMinor, "MouseEventFilter" );
        qmlRegisterType<FlickableScrollHandler>( uri, versionMajor, versionMinor, "FlickableScrollHandler" );

        qmlRegisterUncreatableType<ControlbarProfileModel>(uri, versionMajor, versionMinor, "ControlbarProfileModel", "");
        qmlRegisterUncreatableType<ControlbarProfile>(uri, versionMajor, versionMinor, "ControlbarProfile", "");
        qmlRegisterUncreatableType<PlayerControlbarModel>(uri, versionMajor, versionMinor, "PlayerControlbarModel", "");
        qmlRegisterUncreatableType<ControlListModel>( uri, versionMajor, versionMinor, "ControlListModel", "" );
        qmlRegisterSingletonType(uri, versionMajor, versionMinor, "PlayerListModel", PlayerControlbarModel::getPlaylistIdentifierListModel);

        qmlRegisterType<StringListMenu>( uri, versionMajor, versionMinor, "StringListMenu" );
        qmlRegisterType<SortMenu>( uri, versionMajor, versionMinor, "SortMenu" );
        qmlRegisterType<SortMenuVideo>( uri, versionMajor, versionMinor, "SortMenuVideo" );
        qmlRegisterType<QmlGlobalMenu>( uri, versionMajor, versionMinor, "QmlGlobalMenu" );
        qmlRegisterType<QmlMenuBar>( uri, versionMajor, versionMinor, "QmlMenuBar" );
        qmlRegisterType<NetworkMediaContextMenu>( uri, versionMajor, versionMinor, "NetworkMediaContextMenu" );
        qmlRegisterType<NetworkDeviceContextMenu>( uri, versionMajor, versionMinor, "NetworkDeviceContextMenu" );
        qmlRegisterType<PlaylistContextMenu>( uri, versionMajor, versionMinor, "PlaylistContextMenu" );
        qmlRegisterType<SortFilterProxyModel>( uri, versionMajor, versionMinor, "SortFilterProxyModel" );

        qRegisterMetaType<QList<QQmlError>>("QList<QQmlError>");

        qmlRegisterUncreatableType<NavigationAttached>( uri, versionMajor, versionMinor, "Navigation", "Navigation is only available via attached properties");


        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* const uri = "org.videolan.compat";
        const int versionMajor = 0;
        const int versionMinor = 1;

        qmlRegisterModule(uri, versionMajor, versionMinor);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        qmlRegisterType(QUrl("qrc:///util/BindingRev14.qml"), uri, versionMajor, versionMinor, "BindingCompat");
#else
        qmlRegisterType(QUrl("qrc:///util/BindingRev8.qml"), uri, versionMajor, versionMinor, "BindingCompat");
#endif

        qmlProtectModule(uri, versionMajor);
    }

    {
        // Custom controls

        const char* uri = "org.videolan.controls";
        const int versionMajor = 0;
        const int versionMinor = 1;

        qmlRegisterType<RoundImage>( uri, versionMajor, versionMinor, "RoundImage" );


        qmlProtectModule(uri, versionMajor);
    }

    if (m_mainCtx->hasMediaLibrary())
    {
        const char* uri = "org.videolan.medialib";
        const int versionMajor = 0;
        const int versionMinor = 1;

        qmlRegisterSingletonType<MediaLib>(uri, versionMajor, versionMinor, "MediaLib", SingletonRegisterHelper<MediaLib>::callback);

        qRegisterMetaType<MLItemId>();
        qmlRegisterUncreatableType<MLItemId>( uri, versionMajor, versionMinor, "MLItemId", "");
        qmlRegisterUncreatableType<MLBaseModel>( uri, versionMajor, versionMinor, "MLModel", "ML Base Model is uncreatable." );
        qmlRegisterType<MLAlbumModel>( uri, versionMajor, versionMinor, "MLAlbumModel" );
        qmlRegisterType<MLArtistModel>( uri, versionMajor, versionMinor, "MLArtistModel" );
        qmlRegisterType<MLAlbumTrackModel>( uri, versionMajor, versionMinor, "MLAlbumTrackModel" );
        qmlRegisterType<MLGenreModel>( uri, versionMajor, versionMinor, "MLGenreModel" );
        qmlRegisterType<MLUrlModel>( uri, versionMajor, versionMinor, "MLUrlModel" );
        qmlRegisterType<MLVideoModel>( uri, versionMajor, versionMinor, "MLVideoModel" );
        qmlRegisterType<MLRecentsVideoModel>( uri, versionMajor, versionMinor, "MLRecentsVideoModel" );
        qmlRegisterType<MLGroupListModel>( uri, versionMajor, versionMinor, "MLGroupListModel" );
        qmlRegisterType<MLPlaylistListModel>( uri, versionMajor, versionMinor, "MLPlaylistListModel" );
        qmlRegisterType<MLPlaylistModel>( uri, versionMajor, versionMinor, "MLPlaylistModel" );

        qRegisterMetaType<NetworkTreeItem>();
        qmlRegisterType<NetworkMediaModel>( uri, versionMajor, versionMinor, "NetworkMediaModel");
        qmlRegisterType<NetworkDeviceModel>( uri, versionMajor, versionMinor, "NetworkDeviceModel");
        qmlRegisterType<NetworkSourcesModel>( uri, versionMajor, versionMinor, "NetworkSourcesModel");
        qmlRegisterType<ServicesDiscoveryModel>( uri, versionMajor, versionMinor, "ServicesDiscoveryModel");
        qmlRegisterType<MLFoldersModel>( uri, versionMajor, versionMinor, "MLFolderModel");
        qmlRegisterType<MLRecentsModel>( uri, versionMajor, versionMinor, "MLRecentModel" );

        //expose base object, they aren't instanciable from QML side
        registerAnonymousType<MLAlbum>(uri, versionMajor);
        registerAnonymousType<MLArtist>(uri, versionMajor);
        registerAnonymousType<MLAlbumTrack>(uri, versionMajor);

        qmlRegisterType<AlbumContextMenu>( uri, versionMajor, versionMinor, "AlbumContextMenu" );
        qmlRegisterType<ArtistContextMenu>( uri, versionMajor, versionMinor, "ArtistContextMenu" );
        qmlRegisterType<GenreContextMenu>( uri, versionMajor, versionMinor, "GenreContextMenu" );
        qmlRegisterType<AlbumTrackContextMenu>( uri, versionMajor, versionMinor, "AlbumTrackContextMenu" );
        qmlRegisterType<URLContextMenu>( uri, versionMajor, versionMinor, "URLContextMenu" );
        qmlRegisterType<VideoContextMenu>( uri, versionMajor, versionMinor, "VideoContextMenu" );
        qmlRegisterType<GroupListContextMenu>( uri, versionMajor, versionMinor, "GroupListContextMenu" );
        qmlRegisterType<PlaylistListContextMenu>( uri, versionMajor, versionMinor, "PlaylistListContextMenu" );
        qmlRegisterType<PlaylistMediaContextMenu>( uri, versionMajor, versionMinor, "PlaylistMediaContextMenu" );


        qmlProtectModule(uri, versionMajor);
    }
}

void MainUI::onQmlWarning(const QList<QQmlError>& qmlErrors)
{
    for (auto& error: qmlErrors)
    {
        msg_Warn( m_intf, "qml error %s:%i %s", qtu(error.url().toString()), error.line(), qtu(error.description()) );
    }
}
