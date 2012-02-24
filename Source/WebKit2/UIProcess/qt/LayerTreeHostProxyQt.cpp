/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "LayerTreeHostProxy.h"

#include "GraphicsLayerTextureMapper.h"
#include "LayerBackingStore.h"
#include "LayerTreeHostMessages.h"
#include "MainThread.h"
#include "MessageID.h"
#include "ShareableBitmap.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperLayer.h"
#include "UpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebLayerTreeInfo.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <OpenGLShims.h>
#include <QDateTime>

namespace WebKit {

class LayerTreeMessageToRenderer {
public:
    enum Type {
        DeleteLayer,
        CreateTile,
        RemoveTile,
        UpdateTile,
        CreateImage,
        DestroyImage,
        SyncLayerParameters,
        FlushLayerChanges,
        SetRootLayer
    };
    virtual ~LayerTreeMessageToRenderer() { }
    virtual Type type() const = 0;
};

using namespace WebCore;

template<class MessageData, LayerTreeMessageToRenderer::Type messageType>
class LayerTreeMessageToRendererWithData : public LayerTreeMessageToRenderer {
public:
    virtual Type type() const { return messageType; }

    static PassOwnPtr<LayerTreeMessageToRenderer> create(const MessageData& data = MessageData())
    {
        return adoptPtr(new LayerTreeMessageToRendererWithData(data));
    }

    const MessageData& data() const
    {
        return m_data;
    }

private:
    LayerTreeMessageToRendererWithData(const MessageData& data)
        : m_data(data)
    {
    }

    MessageData m_data;
};


namespace {
struct CreateTileMessageData {
    WebLayerID layerID;
    int remoteTileID;
    float scale;
};

struct UpdateTileMessageData {
    WebLayerID layerID;
    int remoteTileID;
    IntRect sourceRect;
    IntRect targetRect;
    RefPtr<ShareableBitmap> bitmap;
};

struct RemoveTileMessageData {
    WebLayerID layerID;
    int remoteTileID;
};

struct CreateImageMessageData {
    int64_t imageID;
    RefPtr<ShareableBitmap> bitmap;
};

struct DestroyImageMessageData {
    int64_t imageID;
};

struct SyncLayerParametersMessageData {
    WebLayerInfo layerInfo;
};

struct EmptyMessageData { };
struct DeleteLayerMessageData {
    WebLayerID layerID;
};
struct SetRootLayerMessageData {
    WebLayerID layerID;
};

class CreateTileMessage
        : public LayerTreeMessageToRendererWithData<CreateTileMessageData, LayerTreeMessageToRenderer::CreateTile> { };
class UpdateTileMessage
        : public LayerTreeMessageToRendererWithData<UpdateTileMessageData, LayerTreeMessageToRenderer::UpdateTile> { };
class RemoveTileMessage
        : public LayerTreeMessageToRendererWithData<RemoveTileMessageData, LayerTreeMessageToRenderer::RemoveTile> { };
class CreateImageMessage
        : public LayerTreeMessageToRendererWithData<CreateImageMessageData, LayerTreeMessageToRenderer::CreateImage> { };
class DestroyImageMessage
        : public LayerTreeMessageToRendererWithData<DestroyImageMessageData, LayerTreeMessageToRenderer::DestroyImage> { };
class FlushLayerChangesMessage
        : public LayerTreeMessageToRendererWithData<EmptyMessageData, LayerTreeMessageToRenderer::FlushLayerChanges> { };
class SyncLayerParametersMessage
        : public LayerTreeMessageToRendererWithData<SyncLayerParametersMessageData, LayerTreeMessageToRenderer::SyncLayerParameters> { };
class DeleteLayerMessage
        : public LayerTreeMessageToRendererWithData<DeleteLayerMessageData, LayerTreeMessageToRenderer::DeleteLayer> { };
class SetRootLayerMessage
        : public LayerTreeMessageToRendererWithData<SetRootLayerMessageData, LayerTreeMessageToRenderer::SetRootLayer> { };
}

PassOwnPtr<GraphicsLayer> LayerTreeHostProxy::createLayer(WebLayerID layerID)
{
    GraphicsLayer* newLayer = new GraphicsLayerTextureMapper(this);
    TextureMapperLayer* layer = toTextureMapperLayer(newLayer);
    layer->setShouldUpdateBackingStoreFromLayer(false);
    return adoptPtr(newLayer);
}

LayerTreeHostProxy::LayerTreeHostProxy(DrawingAreaProxy* drawingAreaProxy)
    : m_drawingAreaProxy(drawingAreaProxy)
    , m_rootLayerID(0)
{
}

LayerTreeHostProxy::~LayerTreeHostProxy()
{
}

// This function needs to be reentrant.
void LayerTreeHostProxy::paintToCurrentGLContext(const TransformationMatrix& matrix, float opacity, const FloatRect& clipRect)
{
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);
    ASSERT(m_textureMapper->accelerationMode() == TextureMapper::OpenGLMode);

    syncRemoteContent();
    GraphicsLayer* currentRootLayer = rootLayer();
    if (!currentRootLayer)
        return;

    TextureMapperLayer* layer = toTextureMapperLayer(currentRootLayer);

    if (!layer)
        return;

    layer->setTextureMapper(m_textureMapper.get());
    m_textureMapper->beginPainting();
    m_textureMapper->bindSurface(0);
    m_textureMapper->beginClip(TransformationMatrix(), clipRect);

    if (currentRootLayer->opacity() != opacity || currentRootLayer->transform() != matrix) {
        currentRootLayer->setOpacity(opacity);
        currentRootLayer->setTransform(matrix);
        currentRootLayer->syncCompositingStateForThisLayerOnly();
    }

    layer->paint();
    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    syncAnimations();
}

void LayerTreeHostProxy::syncAnimations()
{
    TextureMapperLayer* layer = toTextureMapperLayer(rootLayer());
    ASSERT(layer);

    layer->syncAnimationsRecursively();
    if (layer->descendantsOrSelfHaveRunningAnimations())
        updateViewport();
}

void LayerTreeHostProxy::paintToGraphicsContext(QPainter* painter)
{
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create();
    ASSERT(m_textureMapper->accelerationMode() == TextureMapper::SoftwareMode);
    syncRemoteContent();
    TextureMapperLayer* layer = toTextureMapperLayer(rootLayer());

    if (!layer)
        return;

    GraphicsContext graphicsContext(painter);
    m_textureMapper->setGraphicsContext(&graphicsContext);
    m_textureMapper->beginPainting();
    m_textureMapper->bindSurface(0);
    layer->paint();
    m_textureMapper->endPainting();
    m_textureMapper->setGraphicsContext(0);
}

void LayerTreeHostProxy::updateViewport()
{
    m_drawingAreaProxy->updateViewport();
}

void LayerTreeHostProxy::syncLayerParameters(const WebLayerInfo& layerInfo)
{
    WebLayerID id = layerInfo.id;
    ensureLayer(id);
    LayerMap::iterator it = m_layers.find(id);
    GraphicsLayer* layer = it->second;
    bool needsToUpdateImageTiles = layerInfo.imageIsUpdated || (layerInfo.contentsRect != layer->contentsRect() && layerInfo.imageBackingStoreID);

    layer->setName(layerInfo.name);

    layer->setReplicatedByLayer(layerByID(layerInfo.replica));
    layer->setMaskLayer(layerByID(layerInfo.mask));

    layer->setPosition(layerInfo.pos);
    layer->setSize(layerInfo.size);
    layer->setTransform(layerInfo.transform);
    layer->setAnchorPoint(layerInfo.anchorPoint);
    layer->setChildrenTransform(layerInfo.childrenTransform);
    layer->setBackfaceVisibility(layerInfo.backfaceVisible);
    layer->setContentsOpaque(layerInfo.contentsOpaque);
    layer->setContentsRect(layerInfo.contentsRect);
    layer->setDrawsContent(layerInfo.drawsContent);

    if (needsToUpdateImageTiles)
        assignImageToLayer(layer, layerInfo.imageBackingStoreID);

    // Never make the root layer clip.
    layer->setMasksToBounds(layerInfo.isRootLayer ? false : layerInfo.masksToBounds);
    layer->setOpacity(layerInfo.opacity);
    layer->setPreserves3D(layerInfo.preserves3D);
    Vector<GraphicsLayer*> children;

    for (size_t i = 0; i < layerInfo.children.size(); ++i) {
        WebLayerID childID = layerInfo.children[i];
        GraphicsLayer* child = layerByID(childID);
        if (!child) {
            child = createLayer(childID).leakPtr();
            m_layers.add(childID, child);
        }
        children.append(child);
    }
    layer->setChildren(children);

    for (size_t i = 0; i < layerInfo.animations.size(); ++i) {
        const WebKit::WebLayerAnimation anim = layerInfo.animations[i];

        switch (anim.operation) {
        case WebKit::WebLayerAnimation::AddAnimation: {
            const IntSize boxSize = anim.boxSize;
            layer->addAnimation(anim.keyframeList, boxSize, anim.animation.get(), anim.name, anim.startTime);
            break;
        }
        case WebKit::WebLayerAnimation::RemoveAnimation:
            layer->removeAnimation(anim.name);
            break;
        case WebKit::WebLayerAnimation::PauseAnimation:
            double offset = WTF::currentTime() - anim.startTime;
            layer->pauseAnimation(anim.name, offset);
            break;
        }
    }

    if (layerInfo.isRootLayer && m_rootLayerID != id)
        setRootLayerID(id);
}

void LayerTreeHostProxy::deleteLayer(WebLayerID layerID)
{
    GraphicsLayer* layer = layerByID(layerID);
    if (!layer)
        return;

    layer->removeFromParent();
    m_layers.remove(layerID);
    delete layer;
}


void LayerTreeHostProxy::ensureLayer(WebLayerID id)
{
    // We have to leak the new layer's pointer and manage it ourselves,
    // because OwnPtr is not copyable.
    if (m_layers.find(id) == m_layers.end())
        m_layers.add(id, createLayer(id).leakPtr());
}

void LayerTreeHostProxy::setRootLayerID(WebLayerID layerID)
{
    if (layerID == m_rootLayerID)
        return;

    m_rootLayerID = layerID;

    m_rootLayer->removeAllChildren();

    if (!layerID)
        return;

    GraphicsLayer* layer = layerByID(layerID);
    if (!layer)
        return;

    m_rootLayer->addChild(layer);
}

PassRefPtr<LayerBackingStore> LayerTreeHostProxy::getBackingStore(WebLayerID id)
{
    ensureLayer(id);
    TextureMapperLayer* layer = toTextureMapperLayer(layerByID(id));
    RefPtr<LayerBackingStore> backingStore = static_cast<LayerBackingStore*>(layer->backingStore().get());
    if (!backingStore) {
        backingStore = LayerBackingStore::create();
        layer->setBackingStore(backingStore.get());
    }
    ASSERT(backingStore);
    return backingStore;
}

void LayerTreeHostProxy::createTile(WebLayerID layerID, int tileID, float scale)
{
    getBackingStore(layerID)->createTile(tileID, scale);
}

void LayerTreeHostProxy::removeTile(WebLayerID layerID, int tileID)
{
    getBackingStore(layerID)->removeTile(tileID);
}

void LayerTreeHostProxy::updateTile(WebLayerID layerID, int tileID, const IntRect& sourceRect, const IntRect& targetRect, ShareableBitmap* bitmap)
{
    RefPtr<LayerBackingStore> backingStore = getBackingStore(layerID);
    backingStore->updateTile(tileID, sourceRect, targetRect, bitmap);
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void LayerTreeHostProxy::createImage(int64_t imageID, ShareableBitmap* bitmap)
{
    RefPtr<TextureMapperTiledBackingStore> backingStore = TextureMapperTiledBackingStore::create();
    backingStore->updateContents(m_textureMapper.get(), bitmap->createImage().get(), BitmapTexture::BGRAFormat);
    m_directlyCompositedImages.set(imageID, backingStore);
}

void LayerTreeHostProxy::destroyImage(int64_t imageID)
{
    m_directlyCompositedImages.remove(imageID);
}

void LayerTreeHostProxy::assignImageToLayer(GraphicsLayer* layer, int64_t imageID)
{
    HashMap<int64_t, RefPtr<TextureMapperBackingStore> >::iterator it = m_directlyCompositedImages.find(imageID);
    ASSERT(it != m_directlyCompositedImages.end());
    layer->setContentsToMedia(it->second.get());
}

void LayerTreeHostProxy::swapBuffers()
{
    HashSet<RefPtr<LayerBackingStore> >::iterator end = m_backingStoresWithPendingBuffers.end();
    for (HashSet<RefPtr<LayerBackingStore> >::iterator it = m_backingStoresWithPendingBuffers.begin(); it != end; ++it)
        (*it)->swapBuffers(m_textureMapper.get());

    m_backingStoresWithPendingBuffers.clear();
}

void LayerTreeHostProxy::flushLayerChanges()
{
    m_rootLayer->syncCompositingState(FloatRect());
    swapBuffers();

    // The pending tiles state is on its way for the screen, tell the web process to render the next one.
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::RenderNextFrame(), m_drawingAreaProxy->page()->pageID());
}

void LayerTreeHostProxy::ensureRootLayer()
{
    if (m_rootLayer)
        return;
    m_rootLayer = createLayer(InvalidWebLayerID);
    m_rootLayer->setMasksToBounds(false);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));

    // The root layer should not have zero size, or it would be optimized out.
    m_rootLayer->setSize(FloatSize(1.0, 1.0));
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);
    toTextureMapperLayer(m_rootLayer.get())->setTextureMapper(m_textureMapper.get());
}

void LayerTreeHostProxy::syncRemoteContent()
{
    // We enqueue messages and execute them during paint, as they require an active GL context.
    ensureRootLayer();

    while (OwnPtr<LayerTreeMessageToRenderer> nextMessage = m_messagesToRenderer.tryGetMessage()) {
        switch (nextMessage->type()) {
        case LayerTreeMessageToRenderer::SetRootLayer: {
            const SetRootLayerMessageData& data = static_cast<SetRootLayerMessage*>(nextMessage.get())->data();
            setRootLayerID(data.layerID);
            break;
        }

        case LayerTreeMessageToRenderer::DeleteLayer: {
            const DeleteLayerMessageData& data = static_cast<DeleteLayerMessage*>(nextMessage.get())->data();
            deleteLayer(data.layerID);
            break;
        }

        case LayerTreeMessageToRenderer::SyncLayerParameters: {
            const SyncLayerParametersMessageData& data = static_cast<SyncLayerParametersMessage*>(nextMessage.get())->data();
            syncLayerParameters(data.layerInfo);
            break;
        }

        case LayerTreeMessageToRenderer::CreateTile: {
            const CreateTileMessageData& data = static_cast<CreateTileMessage*>(nextMessage.get())->data();
            createTile(data.layerID, data.remoteTileID, data.scale);
            break;
        }

        case LayerTreeMessageToRenderer::RemoveTile: {
            const RemoveTileMessageData& data = static_cast<RemoveTileMessage*>(nextMessage.get())->data();
            removeTile(data.layerID, data.remoteTileID);
            break;
        }

        case LayerTreeMessageToRenderer::UpdateTile: {
            const UpdateTileMessageData& data = static_cast<UpdateTileMessage*>(nextMessage.get())->data();
            updateTile(data.layerID, data.remoteTileID, data.sourceRect, data.targetRect, data.bitmap.get());
            break;
        }

        case LayerTreeMessageToRenderer::CreateImage: {
            const CreateImageMessageData& data = static_cast<CreateImageMessage*>(nextMessage.get())->data();
            createImage(data.imageID, data.bitmap.get());
            break;
        }

        case LayerTreeMessageToRenderer::DestroyImage: {
            const DestroyImageMessageData& data = static_cast<DestroyImageMessage*>(nextMessage.get())->data();
            destroyImage(data.imageID);
            break;
        }

        case LayerTreeMessageToRenderer::FlushLayerChanges:
            flushLayerChanges();
            break;
        }
    }
}

void LayerTreeHostProxy::pushUpdateToQueue(PassOwnPtr<LayerTreeMessageToRenderer> message)
{
    m_messagesToRenderer.append(message);
    updateViewport();
}

void LayerTreeHostProxy::createTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo& updateInfo)
{
    CreateTileMessageData data;
    data.layerID = layerID;
    data.remoteTileID = tileID;
    data.scale = updateInfo.updateScaleFactor;
    pushUpdateToQueue(CreateTileMessage::create(data));
    updateTileForLayer(layerID, tileID, updateInfo);
}

void LayerTreeHostProxy::updateTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo& updateInfo)
{
    UpdateTileMessageData data;
    data.layerID = layerID;
    data.remoteTileID = tileID;
    data.bitmap = ShareableBitmap::create(updateInfo.bitmapHandle);
    ASSERT(updateInfo.updateRects.size() == 1);
    data.sourceRect = updateInfo.updateRects.first();
    data.targetRect = updateInfo.updateRectBounds;
    pushUpdateToQueue(UpdateTileMessage::create(data));
}

void LayerTreeHostProxy::removeTileForLayer(int layerID, int tileID)
{
    RemoveTileMessageData data;
    data.layerID = layerID;
    data.remoteTileID = tileID;
    pushUpdateToQueue(RemoveTileMessage::create(data));
}


void LayerTreeHostProxy::deleteCompositingLayer(WebLayerID id)
{
    DeleteLayerMessageData data;
    data.layerID = id;
    pushUpdateToQueue(DeleteLayerMessage::create(data));
}

void LayerTreeHostProxy::setRootCompositingLayer(WebLayerID id)
{
    SetRootLayerMessageData data;
    data.layerID = id;
    pushUpdateToQueue(SetRootLayerMessage::create(data));
}

void LayerTreeHostProxy::syncCompositingLayerState(const WebLayerInfo& info)
{
    SyncLayerParametersMessageData data;
    data.layerInfo = info;
    pushUpdateToQueue(SyncLayerParametersMessage::create(data));
}

void LayerTreeHostProxy::didRenderFrame()
{
    pushUpdateToQueue(FlushLayerChangesMessage::create());
    updateViewport();
}

void LayerTreeHostProxy::createDirectlyCompositedImage(int64_t key, const WebKit::ShareableBitmap::Handle& handle)
{
    CreateImageMessageData data;
    data.imageID = key;
    data.bitmap = ShareableBitmap::create(handle);
    pushUpdateToQueue(CreateImageMessage::create(data));
}

void LayerTreeHostProxy::destroyDirectlyCompositedImage(int64_t key)
{
    DestroyImageMessageData data;
    data.imageID = key;
    pushUpdateToQueue(DestroyImageMessage::create(data));
}

void LayerTreeHostProxy::setVisibleContentsRectForPanning(const IntRect& rect, const FloatPoint& trajectoryVector)
{
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::SetVisibleContentsRectForPanning(rect, trajectoryVector), m_drawingAreaProxy->page()->pageID());
}

void LayerTreeHostProxy::setVisibleContentsRectForScaling(const IntRect& rect, float scale)
{
    m_visibleContentsRect = rect;
    m_contentsScale = scale;
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::SetVisibleContentsRectForScaling(rect, scale), m_drawingAreaProxy->page()->pageID());
}

void LayerTreeHostProxy::purgeGLResources()
{
    TextureMapperLayer* layer = toTextureMapperLayer(rootLayer());

    if (layer)
        layer->clearBackingStoresRecursive();

    m_directlyCompositedImages.clear();
    m_textureMapper.clear();
    m_backingStoresWithPendingBuffers.clear();
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::PurgeBackingStores(), m_drawingAreaProxy->page()->pageID());
}

}
#endif