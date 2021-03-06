// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <string>

#include "base/ios/block_types.h"
#include "base/values.h"

#import "ios/web/public/web_state/web_state.h"

namespace web {
namespace test {

// Synchronously returns the result of executed JavaScript.
std::unique_ptr<base::Value> ExecuteJavaScript(web::WebState* web_state,
                                               const std::string& script);

// Returns the CGRect, in the coordinate system of web_state's view, that
// encloses the element with |element_id| in |web_state|'s webview.
// There is no guarantee that the CGRect returned is inside the current window;
// callers should check and act accordingly (scrolling the webview, perhaps).
// Returns CGRectNull if no element could be found.
CGRect GetBoundingRectOfElementWithId(web::WebState* web_state,
                                      const std::string& element_id);

// Returns whether the element with |element_id| in the passed |web_state| has
// been tapped using a JavaScript click() event.
bool TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id);

// Returns whether the element with |element_id| in the passed |web_state| has
// been focused using a JavaScript focus() event.
bool FocusWebViewElementWithId(web::WebState* web_state,
                               const std::string& element_id);

// Returns whether the form with |form_id| in the passed |web_state| has been
// submitted using a JavaScript submit() event.
bool SubmitWebViewFormWithId(web::WebState* web_state,
                             const std::string& form_id);
}  // namespace test
}  // namespace web
