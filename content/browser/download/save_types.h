// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_TYPES_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_TYPES_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "url/gurl.h"

namespace content {

// Map from save_item_id into final file path.
typedef std::map<int, base::FilePath> FinalNamesMap;

// This structure is used to handle and deliver some info
// when processing each save item job.
struct SaveFileCreateInfo {
  enum SaveFileSource {
    // This type indicates the source is not set.
    SAVE_FILE_FROM_UNKNOWN = -1,
    // This type indicates the save item needs to be retrieved from the network.
    SAVE_FILE_FROM_NET = 0,
    // This type indicates the save item needs to be retrieved from serializing
    // DOM.
    SAVE_FILE_FROM_DOM,
    // This type indicates the save item needs to be retrieved from local file
    // system.
    SAVE_FILE_FROM_FILE
  };

  // Constructor for SAVE_FILE_FROM_DOM and/or SAVE_FILE_FROM_FILE.
  SaveFileCreateInfo(const base::FilePath& path,
                     const GURL& url,
                     int save_item_id,
                     int save_package_id,
                     int render_process_id,
                     int render_frame_routing_id,
                     SaveFileSource save_source);

  // Constructor for SAVE_FILE_FROM_NET case.
  SaveFileCreateInfo(const GURL& url,
                     const GURL& final_url,
                     int save_item_id,
                     int save_package_id,
                     int render_process_id,
                     int render_frame_routing_id,
                     int request_id,
                     const std::string& content_disposition,
                     int64 total_bytes);

  ~SaveFileCreateInfo();

  // SaveItem fields.
  // The local file path of saved file.
  base::FilePath path;
  // Original URL of the saved resource.
  GURL url;
  // Final URL of the saved resource since some URL might be redirected.
  GURL final_url;
  // The unique identifier of SaveItem object associated with this job.
  int save_item_id;
  // ID of SavePackage object.
  int save_package_id;
  // IDs for looking up the contents we are associated with.
  int render_process_id;
  int render_frame_routing_id;
  // Handle for informing the ResourceDispatcherHost of a UI based cancel.
  int request_id;
  // Disposition info from HTTP response.
  std::string content_disposition;
  // Total bytes of saved file.
  int64 total_bytes;
  // Source type of saved file.
  SaveFileSource save_source;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_TYPES_H_
