/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollingTree_h
#define ScrollingTree_h

#if ENABLE(THREADED_SCROLLING)

#include "Region.h"
#include <wtf/Functional.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class IntPoint;
class PlatformWheelEvent;
class ScrollingCoordinator;
class ScrollingTreeNode;
class ScrollingTreeState;

// The ScrollingTree class lives almost exclusively on the scrolling thread and manages the
// hierarchy of scrollable regions on the page. It's also responsible for dispatching events
// to the correct scrolling tree nodes or dispatching events back to the ScrollingCoordinator
// object on the main thread if they can't be handled on the scrolling thread for various reasons.
class ScrollingTree : public ThreadSafeRefCounted<ScrollingTree> {
public:
    static PassRefPtr<ScrollingTree> create(ScrollingCoordinator*);
    ~ScrollingTree();

    enum EventResult {
        DidNotHandleEvent,
        DidHandleEvent,
        SendToMainThread
    };

    // Can be called from any thread. Will try to handle the wheel event on the scrolling thread.
    // Returns true if the wheel event can be handled on the scrolling thread and false if the
    // event must be sent again to the WebCore event handler.
    EventResult tryToHandleWheelEvent(const PlatformWheelEvent&);

    // Can be called from any thread. Will update the back forward state of the page, used for rubber-banding.
    void updateBackForwardState(bool canGoBack, bool canGoForward);

    // Must be called from the scrolling thread. Handles the wheel event.
    void handleWheelEvent(const PlatformWheelEvent&);

    void setMainFrameScrollPosition(const IntPoint&);

    void invalidate();
    void commitNewTreeState(PassOwnPtr<ScrollingTreeState>);

    void setMainFramePinState(bool pinnedToTheLeft, bool pinnedToTheRight);

    void updateMainFrameScrollPosition(const IntPoint& scrollPosition);
    void updateMainFrameScrollPositionAndScrollLayerPosition(const IntPoint& scrollPosition);

    bool canGoBack();
    bool canGoForward();

    bool willWheelEventStartSwipeGesture(const PlatformWheelEvent&);

private:
    explicit ScrollingTree(ScrollingCoordinator*);

    RefPtr<ScrollingCoordinator> m_scrollingCoordinator;
    OwnPtr<ScrollingTreeNode> m_rootNode;

    Mutex m_mutex;
    Region m_nonFastScrollableRegion;
    IntPoint m_mainFrameScrollPosition;
    bool m_hasWheelEventHandlers;

    Mutex m_swipeStateMutex;
    bool m_canGoBack;
    bool m_canGoForward;
    bool m_mainFramePinnedToTheLeft;
    bool m_mainFramePinnedToTheRight;
};

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)

#endif // ScrollingTree_h