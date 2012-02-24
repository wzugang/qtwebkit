/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#import <WebCore/NotificationPresenter.h>

#if ENABLE(NOTIFICATIONS)
#import <WebCore/Notification.h>
#import <wtf/HashMap.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#endif

namespace WebCore {
class Notification;
class ScriptExecutionContext;
class VoidCallback;
}

@class WebNotification;
@class WebView;

class WebNotificationClient : public WebCore::NotificationPresenter {
public:
    WebNotificationClient(WebView *);
    WebView *webView() { return m_webView; }

private:
    virtual bool show(WebCore::Notification*) OVERRIDE;
    virtual void cancel(WebCore::Notification*) OVERRIDE;
    virtual void clearNotifications(WebCore::ScriptExecutionContext*) OVERRIDE;
    virtual void notificationObjectDestroyed(WebCore::Notification*) OVERRIDE;
    virtual void notificationControllerDestroyed() OVERRIDE;
    virtual void requestPermission(WebCore::ScriptExecutionContext*, PassRefPtr<WebCore::VoidCallback>) OVERRIDE;
    virtual void cancelRequestsForPermission(WebCore::ScriptExecutionContext*) OVERRIDE { }
    virtual WebCore::NotificationPresenter::Permission checkPermission(WebCore::ScriptExecutionContext*) OVERRIDE;

    WebView *m_webView;
#if ENABLE(NOTIFICATIONS)
    HashMap<RefPtr<WebCore::Notification>, RetainPtr<WebNotification> > m_notificationMap;
    
    typedef HashMap<RefPtr<WebCore::ScriptExecutionContext>, Vector<RetainPtr<WebNotification> > > NotificationContextMap;
    NotificationContextMap m_notificationContextMap;
#endif
};