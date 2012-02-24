/*
 * Copyright 2011 Adobe Systems Incorporated. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef WebKitCSSShaderValue_h
#define WebKitCSSShaderValue_h

#if ENABLE(CSS_SHADERS)

#include "CSSValue.h"

namespace WebCore {

class CachedResourceLoader;
class StyleCachedShader;
class StyleShader;

class WebKitCSSShaderValue : public CSSValue {
public:
    static PassRefPtr<WebKitCSSShaderValue> create(const String& url) { return adoptRef(new WebKitCSSShaderValue(url)); }
    ~WebKitCSSShaderValue();

    StyleCachedShader* cachedShader(CachedResourceLoader*);
    StyleShader* cachedOrPendingShader();

    String customCssText() const;

private:
    WebKitCSSShaderValue(const String& url);

    String m_url;
    RefPtr<StyleShader> m_shader;
    bool m_accessedShader;
};

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)

#endif // WebKitCSSShaderValue_h