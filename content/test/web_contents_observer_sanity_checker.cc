// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/web_contents_observer_sanity_checker.h"

#include "base/strings/stringprintf.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"

namespace content {

namespace {

const char kWebContentsObserverSanityCheckerKey[] =
    "WebContentsObserverSanityChecker";

}  // namespace

// static
void WebContentsObserverSanityChecker::Enable(WebContents* web_contents) {
  if (web_contents->GetUserData(&kWebContentsObserverSanityCheckerKey))
    return;
  web_contents->SetUserData(&kWebContentsObserverSanityCheckerKey,
                            new WebContentsObserverSanityChecker(web_contents));
}

void WebContentsObserverSanityChecker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  std::pair<int, int> routing_pair =
      std::make_pair(render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID());
  bool frame_exists = !live_routes_.insert(routing_pair).second;
  deleted_routes_.erase(routing_pair);

  if (frame_exists) {
    CHECK(false) << "RenderFrameCreated called more than once for routing pair:"
                 << Format(render_frame_host);
  }

  CHECK(render_frame_host->GetProcess()->HasConnection())
      << "RenderFrameCreated was called for a RenderFrameHost whose render "
         "process is not currently live, so there's no way for the RenderFrame "
         "to have been created.";
  CHECK(
      static_cast<RenderFrameHostImpl*>(render_frame_host)->IsRenderFrameLive())
      << "RenderFrameCreated called on for a RenderFrameHost that thinks it is "
         "not alive.";

  // Any child frame must be in the same BrowsingInstance as its parent.
  if (render_frame_host->GetParent()) {
    CHECK(render_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
        render_frame_host->GetParent()->GetSiteInstance()));
  }
}

void WebContentsObserverSanityChecker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  std::pair<int, int> routing_pair =
      std::make_pair(render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID());
  bool was_live = !!live_routes_.erase(routing_pair);
  bool was_dead_already = !deleted_routes_.insert(routing_pair).second;

  if (was_dead_already) {
    CHECK(false) << "RenderFrameDeleted called more than once for routing pair "
                 << Format(render_frame_host);
  } else if (!was_live) {
// TODO(nick): Clients can easily ignore an unrecognized object, but it
// would be useful from a finding-bugs perspective if we could enable this
// check.
#if 0
    CHECK(false) << "RenderFrameDeleted called for routing pair "
                 << Format(render_frame_host)
                 << " for which RenderFrameCreated was never called";
#endif
  }

  // All players should have been paused by this point.
  for (const auto& id : active_media_players_)
    CHECK_NE(id.first, render_frame_host);
}

void WebContentsObserverSanityChecker::RenderFrameForInterstitialPageCreated(
    RenderFrameHost* render_frame_host) {
  // TODO(nick): Record this.
}

void WebContentsObserverSanityChecker::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  CHECK(new_host);
  CHECK_NE(new_host, old_host);

  if (old_host) {
    std::pair<int, int> routing_pair =
        std::make_pair(old_host->GetProcess()->GetID(),
                       old_host->GetRoutingID());
    bool old_did_exist = !!current_hosts_.erase(routing_pair);
    if (!old_did_exist) {
      CHECK(false)
          << "RenderFrameHostChanged called with old host that did not exist:"
          << Format(old_host);
    }
  }

  std::pair<int, int> routing_pair =
      std::make_pair(new_host->GetProcess()->GetID(),
                     new_host->GetRoutingID());
  bool host_exists = !current_hosts_.insert(routing_pair).second;
  if (host_exists) {
    CHECK(false)
        << "RenderFrameHostChanged called more than once for routing pair:"
        << Format(new_host);
  }
}

void WebContentsObserverSanityChecker::FrameDeleted(
    RenderFrameHost* render_frame_host) {
  // A frame can be deleted before RenderFrame in the renderer process is
  // created, so there is not much that can be enforced here.
  CHECK(!web_contents_destroyed_);
}

void WebContentsObserverSanityChecker::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(!NavigationIsOngoing(navigation_handle));

  CHECK(navigation_handle->GetNetErrorCode() == net::OK);
  CHECK(!navigation_handle->HasCommitted());
  CHECK(!navigation_handle->IsErrorPage());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());

  ongoing_navigations_.insert(navigation_handle);
}

void WebContentsObserverSanityChecker::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  CHECK(navigation_handle->GetNetErrorCode() == net::OK);
  CHECK(!navigation_handle->HasCommitted());
  CHECK(!navigation_handle->IsErrorPage());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());
}

void WebContentsObserverSanityChecker::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  CHECK(!navigation_handle->HasCommitted());
  CHECK(navigation_handle->GetRenderFrameHost());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());
  CHECK(navigation_handle->GetRenderFrameHost() != nullptr);
}

void WebContentsObserverSanityChecker::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  CHECK(!(navigation_handle->HasCommitted() &&
          !navigation_handle->IsErrorPage()) ||
        navigation_handle->GetNetErrorCode() == net::OK);
  CHECK(!(navigation_handle->HasCommitted() &&
          navigation_handle->IsErrorPage()) ||
        navigation_handle->GetNetErrorCode() != net::OK);
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());

  CHECK(!navigation_handle->HasCommitted() ||
        navigation_handle->GetRenderFrameHost() != nullptr);

  ongoing_navigations_.erase(navigation_handle);
}

void WebContentsObserverSanityChecker::DidStartProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidFailProvisionalLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidNavigateMainFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  AssertMainFrameExists();
}

void WebContentsObserverSanityChecker::DidNavigateAnyFrame(
    RenderFrameHost* render_frame_host,
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DocumentAvailableInMainFrame() {
  AssertMainFrameExists();
}

void WebContentsObserverSanityChecker::DocumentOnLoadCompletedInMainFrame() {
  AssertMainFrameExists();
}

void WebContentsObserverSanityChecker::DocumentLoadedInFrame(
    RenderFrameHost* render_frame_host) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidFinishLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidFailLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidGetRedirectForResourceRequest(
    RenderFrameHost* render_frame_host,
    const ResourceRedirectDetails& details) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverSanityChecker::DidOpenRequestedURL(
    WebContents* new_contents,
    RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition) {
  AssertRenderFrameExists(source_render_frame_host);
}

void WebContentsObserverSanityChecker::MediaStartedPlaying(
    const MediaPlayerId& id) {
  CHECK(!web_contents_destroyed_);
  CHECK(std::find(active_media_players_.begin(), active_media_players_.end(),
                  id) == active_media_players_.end());
  active_media_players_.push_back(id);
}

void WebContentsObserverSanityChecker::MediaStoppedPlaying(
    const MediaPlayerId& id) {
  CHECK(!web_contents_destroyed_);
  CHECK(std::find(active_media_players_.begin(), active_media_players_.end(),
                  id) != active_media_players_.end());
  active_media_players_.erase(std::remove(active_media_players_.begin(),
                                          active_media_players_.end(), id),
                              active_media_players_.end());
}

bool WebContentsObserverSanityChecker::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  // FrameHostMsg_RenderProcessGone is special internal IPC message that
  // should not be leaking outside of RenderFrameHost.
  CHECK(message.type() != FrameHostMsg_RenderProcessGone::ID);

#if !defined(OS_MACOSX)
// TODO(avi): Disabled because of http://crbug.com/445054
  AssertRenderFrameExists(render_frame_host);
#endif
  return false;
}

void WebContentsObserverSanityChecker::WebContentsDestroyed() {
  CHECK(!web_contents_destroyed_);
  web_contents_destroyed_ = true;
  CHECK(ongoing_navigations_.empty());
  CHECK(active_media_players_.empty());
}

WebContentsObserverSanityChecker::WebContentsObserverSanityChecker(
    WebContents* web_contents)
    : WebContentsObserver(web_contents), web_contents_destroyed_(false) {
  // Prime the pump with the initial objects.
  // TODO(nasko): Investigate why this is needed.
  RenderViewCreated(web_contents->GetRenderViewHost());
}

WebContentsObserverSanityChecker::~WebContentsObserverSanityChecker() {
  CHECK(web_contents_destroyed_);
}

void WebContentsObserverSanityChecker::AssertRenderFrameExists(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  std::pair<int, int> routing_pair =
      std::make_pair(render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID());

  bool render_frame_created_happened = live_routes_.count(routing_pair) != 0;
  bool render_frame_deleted_happened = deleted_routes_.count(routing_pair) != 0;

  CHECK(render_frame_created_happened)
      << "A RenderFrameHost pointer was passed to a WebContentsObserver "
      << "method, but WebContentsObserver::RenderFrameCreated was never called "
      << "for that RenderFrameHost: " << Format(render_frame_host);
  CHECK(!render_frame_deleted_happened)
      << "A RenderFrameHost pointer was passed to a WebContentsObserver "
      << "method, but WebContentsObserver::RenderFrameDeleted had already been "
      << "called on that frame:" << Format(render_frame_host);
}

void WebContentsObserverSanityChecker::AssertMainFrameExists() {
  AssertRenderFrameExists(web_contents()->GetMainFrame());
}

std::string WebContentsObserverSanityChecker::Format(
    RenderFrameHost* render_frame_host) {
  return base::StringPrintf(
      "(%d, %d -> %s)", render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(),
      render_frame_host->GetSiteInstance()->GetSiteURL().spec().c_str());
}

bool WebContentsObserverSanityChecker::NavigationIsOngoing(
    NavigationHandle* navigation_handle) {
  auto it = ongoing_navigations_.find(navigation_handle);
  return it != ongoing_navigations_.end();
}

}  // namespace content
