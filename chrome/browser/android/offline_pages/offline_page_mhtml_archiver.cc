// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "components/security_state/security_state_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "net/base/filename_util.h"

namespace offline_pages {
namespace {
const base::FilePath::CharType kMHTMLExtension[] = FILE_PATH_LITERAL("mhtml");
const base::FilePath::CharType kDefaultFileName[] =
    FILE_PATH_LITERAL("offline_page");
const int kTitleLengthMax = 80;
const char kMHTMLFileNameExtension[] = ".mhtml";
const char kFileNameComponentsSeparator[] = "-";
const char kReplaceChars[] = " ";
const char kReplaceWith[] = "_";

void DeleteFileOnFileThread(const base::FilePath& file_path,
                            const base::Closure& callback) {
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(base::IgnoreResult(&base::DeleteFile), file_path,
                 false /* recursive */),
      callback);
}
}  // namespace

// static
std::string OfflinePageMHTMLArchiver::GetFileNameExtension() {
    return kMHTMLFileNameExtension;
}

// static
base::FilePath OfflinePageMHTMLArchiver::GenerateFileName(
    const GURL& url,
    const std::string& title,
    int64_t archive_id) {
  std::string title_part(title.substr(0, kTitleLengthMax));
  std::string suggested_name(
      url.host() + kFileNameComponentsSeparator +
      title_part + kFileNameComponentsSeparator +
      base::Int64ToString(archive_id));

  // Substitute spaces out from title.
  base::ReplaceChars(suggested_name, kReplaceChars, kReplaceWith,
                     &suggested_name);

  return net::GenerateFileName(url,
                               std::string(),  // content disposition
                               std::string(),  // charset
                               suggested_name,
                               std::string(),  // mime-type
                               kDefaultFileName)
      .AddExtension(kMHTMLExtension);
}

OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      weak_ptr_factory_(this) {
  DCHECK(web_contents_);
}

OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver()
    : web_contents_(nullptr),
      weak_ptr_factory_(this) {
}

OfflinePageMHTMLArchiver::~OfflinePageMHTMLArchiver() {
}

void OfflinePageMHTMLArchiver::CreateArchive(
    const base::FilePath& archives_dir,
    int64_t archive_id,
    const CreateArchiveCallback& callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());
  callback_ = callback;

  if (HasConnectionSecurityError()) {
    ReportFailure(ArchiverResult::ERROR_SECURITY_CERTIFICATE);
    return;
  }

  GenerateMHTML(archives_dir, archive_id);
}

void OfflinePageMHTMLArchiver::GenerateMHTML(
    const base::FilePath& archives_dir, int64_t archive_id) {
  if (archives_dir.empty()) {
    DVLOG(1) << "Archive path was empty. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
    return;
  }

  if (!web_contents_) {
    DVLOG(1) << "WebContents is missing. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  if (!web_contents_->GetRenderViewHost()) {
    DVLOG(1) << "RenderViewHost is not created yet. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  // TODO(fgorski): Figure out if the actual URL can be different at
  // the end of MHTML generation. Perhaps we should pull it out after the MHTML
  // is generated.
  GURL url(web_contents_->GetLastCommittedURL());
  base::string16 title(web_contents_->GetTitle());
  base::FilePath file_path(
      archives_dir.Append(
          GenerateFileName(url, base::UTF16ToUTF8(title), archive_id)));

  content::MHTMLGenerationParams params(file_path);
  params.use_binary_encoding = true;

  web_contents_->GenerateMHTML(
      params,
      base::Bind(&OfflinePageMHTMLArchiver::OnGenerateMHTMLDone,
                 weak_ptr_factory_.GetWeakPtr(), url, file_path, title));
}

void OfflinePageMHTMLArchiver::OnGenerateMHTMLDone(
    const GURL& url,
    const base::FilePath& file_path,
    const base::string16& title,
    int64_t file_size) {
  if (file_size < 0) {
    DeleteFileOnFileThread(
        file_path, base::Bind(&OfflinePageMHTMLArchiver::ReportFailure,
                              weak_ptr_factory_.GetWeakPtr(),
                              ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback_, this, ArchiverResult::SUCCESSFULLY_CREATED, url,
                   file_path, title, file_size));
  }
}

bool OfflinePageMHTMLArchiver::HasConnectionSecurityError() {
  ChromeSecurityStateModelClient::CreateForWebContents(web_contents_);
  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(web_contents_);
  DCHECK(model_client);
  security_state::SecurityStateModel::SecurityInfo security_info;
  model_client->GetSecurityInfo(&security_info);
  return security_state::SecurityStateModel::SecurityLevel::DANGEROUS ==
         security_info.security_level;
}

void OfflinePageMHTMLArchiver::ReportFailure(ArchiverResult result) {
  DCHECK(result != ArchiverResult::SUCCESSFULLY_CREATED);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback_, this, result, GURL(), base::FilePath(),
                            base::string16(), 0));
}

}  // namespace offline_pages
