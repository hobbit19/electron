// Copyright (c) 2013 GitHub, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/net/adapter_request_job.h"

#include "browser/net/url_request_string_job.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"

namespace atom {

AdapterRequestJob::AdapterRequestJob(ProtocolHandler* protocol_handler,
                                     net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate)
    : URLRequestJob(request, network_delegate),
      protocol_handler_(protocol_handler),
      weak_factory_(this) {
}

void AdapterRequestJob::Start() {
  DCHECK(!real_job_);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AdapterRequestJob::GetJobTypeInUI,
                 weak_factory_.GetWeakPtr()));
}

void AdapterRequestJob::Kill() {
  DCHECK(real_job_);
  real_job_->Kill();
}

bool AdapterRequestJob::ReadRawData(net::IOBuffer* buf,
                                    int buf_size,
                                    int *bytes_read) {
  DCHECK(real_job_);
  // The ReadRawData is a protected method.
  switch (type_) {
    case REQUEST_STRING_JOB:
      return static_cast<URLRequestStringJob*>(real_job_.get())->
          ReadRawData(buf, buf_size, bytes_read);
    case REQUEST_FILE_JOB:
      return static_cast<net::URLRequestFileJob*>(real_job_.get())->
          ReadRawData(buf, buf_size, bytes_read);
    default:
      return net::URLRequestJob::ReadRawData(buf, buf_size, bytes_read);
  }
}

bool AdapterRequestJob::IsRedirectResponse(GURL* location,
                                           int* http_status_code) {
  DCHECK(real_job_);
  return real_job_->IsRedirectResponse(location, http_status_code);
}

net::Filter* AdapterRequestJob::SetupFilter() const {
  DCHECK(real_job_);
  return real_job_->SetupFilter();
}

bool AdapterRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK(real_job_);
  return real_job_->GetMimeType(mime_type);
}

bool AdapterRequestJob::GetCharset(std::string* charset) {
  DCHECK(real_job_);
  return real_job_->GetCharset(charset);
}

base::WeakPtr<AdapterRequestJob> AdapterRequestJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AdapterRequestJob::CreateErrorJobAndStart(int error_code) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  type_ = REQUEST_ERROR_JOB;
  real_job_ = new net::URLRequestErrorJob(
      request(), network_delegate(), error_code);
  real_job_->Start();
}

void AdapterRequestJob::CreateStringJobAndStart(const std::string& mime_type,
                                                const std::string& charset,
                                                const std::string& data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  type_ = REQUEST_STRING_JOB;
  real_job_ = new URLRequestStringJob(
      request(), network_delegate(), mime_type, charset, data);
  real_job_->Start();
}

void AdapterRequestJob::CreateFileJobAndStart(const base::FilePath& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  type_ = REQUEST_FILE_JOB;
  real_job_ = new net::URLRequestFileJob(request(), network_delegate(), path);
  real_job_->Start();
}

void AdapterRequestJob::CreateJobFromProtocolHandlerAndStart() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(protocol_handler_);
  real_job_ = protocol_handler_->MaybeCreateJob(request(),
                                                network_delegate());
  if (!real_job_)
    CreateErrorJobAndStart(net::ERR_NOT_IMPLEMENTED);
  else
    real_job_->Start();
}

}  // namespace atom
