// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_H_
#define CONTENT_RENDERER_RENDER_WIDGET_H_

#include <deque>
#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/renderer/input/render_widget_input_handler.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/message_delivery_policy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/WebKit/public/web/WebTextInputInfo.h"
#include "third_party/WebKit/public/web/WebTouchAction.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "third_party/WebKit/public/web/WebWidgetClient.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

struct ViewHostMsg_UpdateRect_Params;
struct ViewMsg_Resize_Params;
class ViewHostMsg_UpdateRect;

namespace IPC {
class SyncMessage;
class SyncMessageFilter;
}

namespace blink {
struct WebDeviceEmulationParams;
class WebFrameWidget;
class WebGestureEvent;
class WebKeyboardEvent;
class WebLocalFrame;
class WebMouseEvent;
class WebNode;
struct WebPoint;
class WebTouchEvent;
class WebView;
}

namespace cc {
struct InputHandlerScrollResult;
class OutputSurface;
class SwapPromise;
}

namespace gfx {
class Range;
}

namespace scheduler {
class RenderWidgetSchedulingState;
}

namespace content {
class CompositorDependencies;
class ExternalPopupMenu;
class FrameSwapMessageQueue;
class ImeEventGuard;
class PepperPluginInstanceImpl;
class RenderFrameImpl;
class RenderFrameProxy;
class RenderWidgetCompositor;
class RenderWidgetTest;
class ResizingModeSelector;
struct ContextMenuParams;
struct DidOverscrollParams;
struct WebPluginGeometry;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
//
// RenderWidget is used to implement:
// - RenderViewImpl (deprecated)
// - Fullscreen mode (RenderWidgetFullScreen)
// - Popup "menus" (like the color chooser and date picker)
// - Widgets for frames (for out-of-process iframe support)
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      NON_EXPORTED_BASE(virtual public blink::WebWidgetClient),
      public RenderWidgetInputHandlerDelegate,
      public base::RefCounted<RenderWidget> {
 public:
  // Creates a new RenderWidget.  The opener_id is the routing ID of the
  // RenderView that this widget lives inside.
  static RenderWidget* Create(int32 opener_id,
                              CompositorDependencies* compositor_deps,
                              blink::WebPopupType popup_type,
                              const blink::WebScreenInfo& screen_info);

  // Creates a new RenderWidget that will be attached to a RenderFrame.
  static RenderWidget* CreateForFrame(int routing_id,
                                      bool hidden,
                                      const blink::WebScreenInfo& screen_info,
                                      CompositorDependencies* compositor_deps,
                                      blink::WebLocalFrame* frame);

  // Closes a RenderWidget that was created by |CreateForFrame|.
  // TODO(avi): De-virtualize this once RenderViewImpl has-a RenderWidget.
  // https://crbug.com/545684
  virtual void CloseForFrame();

  int32 routing_id() const { return routing_id_; }
  CompositorDependencies* compositor_deps() const { return compositor_deps_; }
  blink::WebWidget* webwidget() const { return webwidget_; }
  gfx::Size size() const { return size_; }
  bool is_fullscreen_granted() const { return is_fullscreen_granted_; }
  blink::WebDisplayMode display_mode() const { return display_mode_; }
  bool is_hidden() const { return is_hidden_; }
  // Temporary for debugging purposes...
  bool closing() const { return closing_; }
  bool is_swapped_out() { return is_swapped_out_; }
  bool for_oopif() { return for_oopif_; }
  bool has_host_context_menu_location() {
    return has_host_context_menu_location_;
  }
  gfx::Point host_context_menu_location() {
    return host_context_menu_location_;
  }

  // ScreenInfo exposed so it can be passed to subframe RenderWidgets.
  blink::WebScreenInfo screen_info() const { return screen_info_; }

  // Functions to track out-of-process frames for special notifications.
  void RegisterRenderFrameProxy(RenderFrameProxy* proxy);
  void UnregisterRenderFrameProxy(RenderFrameProxy* proxy);

  // Functions to track all RenderFrame objects associated with this
  // RenderWidget.
  void RegisterRenderFrame(RenderFrameImpl* frame);
  void UnregisterRenderFrame(RenderFrameImpl* frame);

#if defined(VIDEO_HOLE)
  void RegisterVideoHoleFrame(RenderFrameImpl* frame);
  void UnregisterVideoHoleFrame(RenderFrameImpl* frame);
#endif  // defined(VIDEO_HOLE)

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // RenderWidgetInputHandlerDelegate
  void FocusChangeComplete() override;
  bool HasTouchEventHandlersAt(const gfx::Point& point) const override;
  void ObserveWheelEventAndResult(const blink::WebMouseWheelEvent& wheel_event,
                                  const gfx::Vector2dF& wheel_unused_delta,
                                  bool event_processed) override;
  void OnDidHandleKeyEvent() override;
  void OnDidOverscroll(const DidOverscrollParams& params) override;
  void OnInputEventAck(scoped_ptr<InputEventAck> input_event_ack) override;
  void UpdateTextInputState(ShowIme show_ime,
                            ChangeSource change_source) override;
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override;
  bool WillHandleMouseEvent(const blink::WebMouseEvent& event) override;

  // blink::WebWidgetClient
  void didAutoResize(const blink::WebSize& new_size) override;
  void initializeLayerTreeView() override;
  blink::WebLayerTreeView* layerTreeView() override;
  void didMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void didFocus() override;
  void didChangeCursor(const blink::WebCursorInfo&) override;
  void closeWidgetSoon() override;
  void show(blink::WebNavigationPolicy) override;
  blink::WebRect windowRect() override;
  void setToolTipText(const blink::WebString& text,
                      blink::WebTextDirection hint) override;
  void setWindowRect(const blink::WebRect&) override;
  blink::WebRect windowResizerRect() override;
  blink::WebRect rootWindowRect() override;
  blink::WebScreenInfo screenInfo() override;
  void resetInputMethod() override;
  void didHandleGestureEvent(const blink::WebGestureEvent& event,
                             bool event_cancelled) override;
  void didOverscroll(const blink::WebFloatSize& unusedDelta,
                     const blink::WebFloatSize& accumulatedRootOverScroll,
                     const blink::WebFloatPoint& position,
                     const blink::WebFloatSize& velocity) override;
  void showImeIfNeeded() override;

  // Override point to obtain that the current input method state and caret
  // position.
  virtual ui::TextInputType GetTextInputType();
  virtual ui::TextInputType WebKitToUiTextInputType(
      blink::WebTextInputType type);

  // Converts the |rect| from Viewport coordinates to Window coordinates.
  // See RenderView::convertViewportToWindow for more details.
  void convertViewportToWindow(blink::WebRect* rect);

#if defined(OS_ANDROID)
  // Notifies that a tap was not consumed, so showing a UI for the unhandled
  // tap may be needed.
  // Performs various checks on the given WebNode to apply heuristics to
  // determine if triggering is appropriate.
  void showUnhandledTapUIIfNeeded(const blink::WebPoint& tapped_position,
                                  const blink::WebNode& tapped_node,
                                  bool page_changed) override;
#endif

  // Begins the compositor's scheduler to start producing frames.
  void StartCompositor();

  // Stop compositing.
  void WillCloseLayerTreeView();

  // Called when a plugin is moved.  These events are queued up and sent with
  // the next paint or scroll message to the host.
  void SchedulePluginMove(const WebPluginGeometry& move);

  // Called when a plugin window has been destroyed, to make sure the currently
  // pending moves don't try to reference it.
  void CleanupWindowInPluginMoves(gfx::PluginWindowHandle window);

  RenderWidgetCompositor* compositor() const;

  const RenderWidgetInputHandler& input_handler() const {
    return input_handler_;
  }

  void SetHandlingInputEventForTesting(bool handling_input_event);

  // When paused in debugger, we send ack for mouse event early. This ensures
  // that we continue receiving mouse moves and pass them to debugger. Returns
  // whether we are paused in mouse move event and have sent the ack.
  bool SendAckForMouseMoveFromDebugger();

  // When resumed from pause in debugger while handling mouse move,
  // we should not send an extra ack (see SendAckForMouseMoveFromDebugger).
  void IgnoreAckForMouseMoveFromDebugger();

  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback);

  // Callback for use with synthetic gestures (e.g. BeginSmoothScroll).
  typedef base::Callback<void()> SyntheticGestureCompletionCallback;

  // Send a synthetic gesture to the browser to be queued to the synthetic
  // gesture controller.
  void QueueSyntheticGesture(
      scoped_ptr<SyntheticGestureParams> gesture_params,
      const SyntheticGestureCompletionCallback& callback);

  // Deliveres |message| together with compositor state change updates. The
  // exact behavior depends on |policy|.
  // This mechanism is not a drop-in replacement for IPC: messages sent this way
  // will not be automatically available to BrowserMessageFilter, for example.
  // FIFO ordering is preserved between messages enqueued with the same
  // |policy|, the ordering between messages enqueued for different policies is
  // undefined.
  //
  // |msg| message to send, ownership of |msg| is transferred.
  // |policy| see the comment on MessageDeliveryPolicy.
  void QueueMessage(IPC::Message* msg, MessageDeliveryPolicy policy);

  // Handle start and finish of IME event guard.
  void OnImeEventGuardStart(ImeEventGuard* guard);
  void OnImeEventGuardFinish(ImeEventGuard* guard);

  // Returns whether we currently should handle an IME event.
  bool ShouldHandleImeEvent();

  // Called by the compositor when page scale animation completed.
  virtual void DidCompletePageScaleAnimation() {}

  // ScreenMetricsEmulator class manages screen emulation inside a render
  // widget. This includes resizing, placing view on the screen at desired
  // position, changing device scale factor, and scaling down the whole
  // widget if required to fit into the browser window.
  class ScreenMetricsEmulator;

  void SetPopupOriginAdjustmentsForEmulation(ScreenMetricsEmulator* emulator);
  gfx::Rect AdjustValidationMessageAnchor(const gfx::Rect& anchor);

  // Indicates that the compositor is about to begin a frame. This is primarily
  // to signal to flow control mechanisms that a frame is beginning, not to
  // perform actual painting work.
  void WillBeginCompositorFrame();

  // Notifies about a compositor frame commit operation having finished.
  virtual void DidCommitCompositorFrame();

  // Notifies that the draw commands for a committed frame have been issued.
  void DidCommitAndDrawCompositorFrame();

  // Notifies that the compositor has posted a swapbuffers operation to the GPU
  // process.
  void DidCompleteSwapBuffers();

  void ScheduleComposite();
  void ScheduleCompositeWithForcedRedraw();

  // Called by the compositor in single-threaded mode when a swap is posted,
  // completes or is aborted.
  void OnSwapBuffersPosted();
  void OnSwapBuffersComplete();
  void OnSwapBuffersAborted();

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  // Called by the compositor to forward a proto that represents serialized
  // compositor state.
  void ForwardCompositorProto(const std::vector<uint8_t>& proto);

  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);

  void OnShowHostContextMenu(ContextMenuParams* params);

  // Checks if the composition range or composition character bounds have been
  // changed. If they are changed, the new value will be sent to the browser
  // process. This method does nothing when the browser process is not able to
  // handle composition range and composition character bounds.
  void UpdateCompositionInfo(bool should_update_range);

  bool host_closing() const { return host_closing_; }

 protected:
  // Friend RefCounted so that the dtor can be non-public. Using this class
  // without ref-counting is an error.
  friend class base::RefCounted<RenderWidget>;
  // For unit tests.
  friend class RenderWidgetTest;

  enum ResizeAck {
    SEND_RESIZE_ACK,
    NO_RESIZE_ACK,
  };

  RenderWidget(CompositorDependencies* compositor_deps,
               blink::WebPopupType popup_type,
               const blink::WebScreenInfo& screen_info,
               bool swapped_out,
               bool hidden,
               bool never_visible);

  ~RenderWidget() override;

  static blink::WebWidget* CreateWebFrameWidget(RenderWidget* render_widget,
                                                blink::WebLocalFrame* frame);

  // Creates a WebWidget based on the popup type.
  static blink::WebWidget* CreateWebWidget(RenderWidget* render_widget);

  // Initializes this view with the given opener.
  bool Init(int32 opener_id);

  // Called by Init and subclasses to perform initialization.
  bool DoInit(int32 opener_id,
              blink::WebWidget* web_widget,
              IPC::SyncMessage* create_widget_message);

  // Sets whether this RenderWidget has been swapped out to be displayed by
  // a RenderWidget in a different process.  If so, no new IPC messages will be
  // sent (only ACKs) and the process is free to exit when there are no other
  // active RenderWidgets.
  void SetSwappedOut(bool is_swapped_out);

  // Allows the process to exit once the unload handler has finished, if there
  // are no other active RenderWidgets.
  void WasSwappedOut();

  void DoDeferredClose();
  void NotifyOnClose();

  // Close the underlying WebWidget.
  virtual void Close();

  // Resizes the render widget.
  void Resize(const gfx::Size& new_size,
              const gfx::Size& physical_backing_size,
              bool top_controls_shrink_blink_size,
              float top_controls_height,
              const gfx::Size& visible_viewport_size,
              const gfx::Rect& resizer_rect,
              bool is_fullscreen_granted,
              blink::WebDisplayMode display_mode,
              ResizeAck resize_ack);
  // Used to force the size of a window when running layout tests.
  void SetWindowRectSynchronously(const gfx::Rect& new_window_rect);
  virtual void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params);
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void SetExternalPopupOriginAdjustmentsForEmulation(
      ExternalPopupMenu* popup, ScreenMetricsEmulator* emulator);
#endif

  // RenderWidget IPC message handlers
  void OnHandleInputEvent(const blink::WebInputEvent* event,
                          const ui::LatencyInfo& latency_info);
  void OnCursorVisibilityChange(bool is_visible);
  void OnMouseCaptureLost();
  virtual void OnSetFocus(bool enable);
  void OnClose();
  void OnCreatingNewAck();
  virtual void OnResize(const ViewMsg_Resize_Params& params);
  void OnEnableDeviceEmulation(const blink::WebDeviceEmulationParams& params);
  void OnDisableDeviceEmulation();
  void OnColorProfile(const std::vector<char>& color_profile);
  void OnChangeResizeRect(const gfx::Rect& resizer_rect);
  virtual void OnWasHidden();
  virtual void OnWasShown(bool needs_repainting,
                          const ui::LatencyInfo& latency_info);
  void OnCreateVideoAck(int32 video_id);
  void OnUpdateVideoAck(int32 video_id);
  void OnRequestMoveAck();
  virtual void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  virtual void OnImeConfirmComposition(const base::string16& text,
                                       const gfx::Range& replacement_range,
                                       bool keep_selection);
  void OnRepaint(gfx::Size size_to_paint);
  void OnSyntheticGestureCompleted();
  void OnSetTextDirection(blink::WebTextDirection direction);
  void OnGetFPS();
  void OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                           const gfx::Rect& window_screen_rect);
  void OnShowImeIfNeeded();
  void OnSetSurfaceIdNamespace(uint32_t surface_id_namespace);
  void OnHandleCompositorProto(const std::vector<uint8_t>& proto);

#if defined(OS_ANDROID)
  // Called when we send IME event that expects an ACK.
  void OnImeEventSentForAck(const blink::WebTextInputInfo& info);

  // Called by the browser process for every required IME acknowledgement.
  void OnImeEventAck();
#endif

  // Notify the compositor about a change in viewport size. This should be
  // used only with auto resize mode WebWidgets, as normal WebWidgets should
  // go through OnResize.
  void AutoResizeCompositor();

  virtual void SetDeviceScaleFactor(float device_scale_factor);
  virtual bool SetDeviceColorProfile(const std::vector<char>& color_profile);
  virtual void ResetDeviceColorProfileForTesting();

  virtual void OnOrientationChange();

  // Override points to notify derived classes that a paint has happened.
  // DidInitiatePaint happens when that has completed, and subsequent rendering
  // won't affect the painted content. DidFlushPaint happens once we've received
  // the ACK that the screen has been updated. For a given paint operation,
  // these overrides will always be called in the order DidInitiatePaint,
  // DidFlushPaint.
  virtual void DidInitiatePaint() {}
  virtual void DidFlushPaint() {}

  virtual GURL GetURLForGraphicsContext3D();

  // Gets the scroll offset of this widget, if this widget has a notion of
  // scroll offset.
  virtual gfx::Vector2d GetScrollOffset();

  // Sets the "hidden" state of this widget.  All accesses to is_hidden_ should
  // use this method so that we can properly inform the RenderThread of our
  // state.
  void SetHidden(bool hidden);

  void DidToggleFullscreen();

  bool next_paint_is_resize_ack() const;
  void set_next_paint_is_resize_ack();
  void set_next_paint_is_repaint_ack();

  // QueueMessage implementation extracted into a static method for easy
  // testing.
  static scoped_ptr<cc::SwapPromise> QueueMessageImpl(
      IPC::Message* msg,
      MessageDeliveryPolicy policy,
      FrameSwapMessageQueue* frame_swap_message_queue,
      scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
      int source_frame_number);

  // Override point to obtain that the current composition character bounds.
  // In the case of surrogate pairs, the character is treated as two characters:
  // the bounds for first character is actual one, and the bounds for second
  // character is zero width rectangle.
  virtual void GetCompositionCharacterBounds(
      std::vector<gfx::Rect>* character_bounds);

  // Returns the range of the text that is being composed or the selection if
  // the composition does not exist.
  virtual void GetCompositionRange(gfx::Range* range);

  // Returns true if the composition range or composition character bounds
  // should be sent to the browser process.
  bool ShouldUpdateCompositionInfo(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds);

  // Override point to obtain that the current input method state about
  // composition text.
  virtual bool CanComposeInline();

  // Set the pending window rect.
  // Because the real render_widget is hosted in another process, there is
  // a time period where we may have set a new window rect which has not yet
  // been processed by the browser.  So we maintain a pending window rect
  // size.  If JS code sets the WindowRect, and then immediately calls
  // GetWindowRect() we'll use this pending window rect as the size.
  void SetPendingWindowRect(const blink::WebRect& r);

  // Check whether the WebWidget has any touch event handlers registered.
  void hasTouchEventHandlers(bool has_handlers) override;

  // Tell the browser about the actions permitted for a new touch point.
  void setTouchAction(blink::WebTouchAction touch_action) override;

  // Called when value of focused text field gets dirty, e.g. value is modified
  // by script, not by user input.
  void didUpdateTextOfFocusedElementByNonUserInput() override;

  // Creates a 3D context associated with this view.
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateGraphicsContext3D(
      bool compositor);

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost. When MSG_ROUTING_NONE, no messages may be sent.
  int32 routing_id_;

  // Dependencies for initializing a compositor, including flags for optional
  // features.
  CompositorDependencies* const compositor_deps_;

  // We are responsible for destroying this object via its Close method.
  // May be NULL when the window is closing.
  blink::WebWidget* webwidget_;

  // This is lazily constructed and must not outlive webwidget_.
  scoped_ptr<RenderWidgetCompositor> compositor_;

  // Set to the ID of the view that initiated creating this view, if any. When
  // the view was initiated by the browser (the common case), this will be
  // MSG_ROUTING_NONE. This is used in determining ownership when opening
  // child tabs. See RenderWidget::createWebViewWithRequest.
  //
  // This ID may refer to an invalid view if that view is closed before this
  // view is.
  int32 opener_id_;

  // The rect where this view should be initially shown.
  gfx::Rect initial_rect_;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  WebCursor current_cursor_;

  // The size of the RenderWidget.
  gfx::Size size_;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  gfx::Size physical_backing_size_;

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar (always false on platforms where URL-bar hiding isn't supported).
  bool top_controls_shrink_blink_size_;

  // The height of the top controls (always 0 on platforms where URL-bar hiding
  // isn't supported).
  float top_controls_height_;

  // The size of the visible viewport in DPI-adjusted pixels.
  gfx::Size visible_viewport_size_;

  // The area that must be reserved for drawing the resize corner.
  gfx::Rect resizer_rect_;

  // Flags for the next ViewHostMsg_UpdateRect message.
  int next_paint_flags_;

  // Whether the WebWidget is in auto resize mode, which is used for example
  // by extension popups.
  bool auto_resize_mode_;

  // True if we need to send an UpdateRect message to notify the browser about
  // an already-completed auto-resize.
  bool need_update_rect_for_auto_resize_;

  // Set to true if we should ignore RenderWidget::Show calls.
  bool did_show_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are never visible, so never produce graphical output.
  const bool compositor_never_visible_;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted_;

  // Indicates the display mode.
  blink::WebDisplayMode display_mode_;

  // It is possible that one ImeEventGuard is nested inside another
  // ImeEventGuard. We keep track of the outermost one, and update it as needed.
  ImeEventGuard* ime_event_guard_;

  // True if we have requested this widget be closed.  No more messages will
  // be sent, except for a Close.
  bool closing_;

  // True if it is known that the host is in the process of being shut down.
  bool host_closing_;

  // Whether this RenderWidget is currently swapped out, such that the view is
  // being rendered by another process.  If all RenderWidgets in a process are
  // swapped out, the process can exit.
  bool is_swapped_out_;

  // TODO(simonhong): Remove this when we enable BeginFrame scheduling for
  // OOPIF(crbug.com/471411).
  // Whether this RenderWidget is for an out-of-process iframe or not.
  bool for_oopif_;

  // Stores information about the current text input.
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_;

  // Stores the current text input flags of |webwidget_|.
  int text_input_flags_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_;

  // The kind of popup this widget represents, NONE if not a popup.
  blink::WebPopupType popup_type_;

  // Holds all the needed plugin window moves for a scroll.
  typedef std::vector<WebPluginGeometry> WebPluginGeometryVector;
  WebPluginGeometryVector plugin_window_moves_;

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_;
  blink::WebRect pending_window_rect_;

  // The screen rects of the view and the window that contains it.
  gfx::Rect view_screen_rect_;
  gfx::Rect window_screen_rect_;

  RenderWidgetInputHandler input_handler_;

  // The time spent in input handlers this frame. Used to throttle input acks.
  base::TimeDelta total_input_handling_time_this_frame_;

  // Properties of the screen hosting this RenderWidget instance.
  blink::WebScreenInfo screen_info_;

  // The device scale factor. This value is computed from the DPI entries in
  // |screen_info_| on some platforms, and defaults to 1 on other platforms.
  float device_scale_factor_;

  // The device color profile on supported platforms.
  std::vector<char> device_color_profile_;

  // State associated with synthetic gestures. Synthetic gestures are processed
  // in-order, so a queue is sufficient to identify the correct state for a
  // completed gesture.
  std::queue<SyntheticGestureCompletionCallback>
      pending_synthetic_gesture_callbacks_;

  uint32 next_output_surface_id_;

#if defined(OS_ANDROID)
  // Indicates value in the focused text field is in dirty state, i.e. modified
  // by script etc., not by user input.
  bool text_field_is_dirty_;

  // Stores the history of text input infos from the last ACK'ed one from the
  // current one. The size is the number of pending ACKs plus one, since we
  // intentionally keep the last ack'd value to know what the browser is
  // currently aware of.
  std::deque<blink::WebTextInputInfo> text_input_info_history_;
#endif

  scoped_ptr<ScreenMetricsEmulator> screen_metrics_emulator_;

  // Popups may be displaced when screen metrics emulation is enabled.
  // These values are used to properly adjust popup position.
  gfx::Point popup_view_origin_for_emulation_;
  gfx::Point popup_screen_origin_for_emulation_;
  float popup_origin_scale_for_emulation_;

  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;
  scoped_ptr<ResizingModeSelector> resizing_mode_selector_;

  // Lists of RenderFrameProxy objects that need to be notified of
  // compositing-related events (e.g. DidCommitCompositorFrame).
  base::ObserverList<RenderFrameProxy> render_frame_proxies_;
#if defined(VIDEO_HOLE)
  base::ObserverList<RenderFrameImpl> video_hole_frames_;
#endif  // defined(VIDEO_HOLE)

  // A list of RenderFrames associated with this RenderWidget. Notifications
  // are sent to each frame in the list for events such as changing
  // visibility state for example.
  base::ObserverList<RenderFrameImpl> render_frames_;

  bool has_host_context_menu_location_;
  gfx::Point host_context_menu_location_;

  scoped_ptr<scheduler::RenderWidgetSchedulingState>
      render_widget_scheduling_state_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_
