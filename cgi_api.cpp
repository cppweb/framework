#include "asio_config.h"

#include "application.h"
#include "url_dispatcher.h"
#include "http_context.h"
#include "http_request.h"
#include "http_response.h"
#include "applications_pool.h"
#include "thread_pool.h"
#include "service.h"
#include "global_config.h"
#include "cgi_api.h"

#include <boost/bind.hpp>


namespace {
	bool inline xdigit(int c) { return ('0'<=c && c<='9') || ('a'<=c && c<='f') || ('A'<=c && c<='F'); }
	char ascii_to_lower(char c)
	{
		if('A'<=c && c<='Z')
			return 'a'+(c-'A');
		return c;
	}
	bool is_prefix_of(char const *prefix,std::string const &s)
	{
		size_t len=strlen(prefix);
		if(s.size() < len)
			return false;
		for(size_t i=0;i<len;i++) {
			if(ascii_to_lower(prefix[i]!=ascii_to_lower(s[i])))
				return false;
		}
		return true;
	}
}


namespace cppcms { namespace impl { namespace cgi {

cppcms::service &connection::service()
{
	return *service_;
}

void connection::on_accepted()
{
	async_read_headers(boost::bind(&connection::load_content,shared_from_this(),_1));
}



void connection::load_content(boost::system::error_code const &e)
{
	if(e) return;

	std::string content_type = getenv("CONTENT_TYPE");
	std::string s_content_length=getenv("CONTENT_LENGTH");

	long long content_length = s_content_length.empty() ? atoll(s_content_length.c_str()) : 0;

	if(content_length < 0)  {
		// TODO log
		return;
	}

	if(is_prefix_of("multipart/form-data",content_type)) {
		// 64 MB
		long long allowed=service().settings().integer("security.multipart_form_data_limit",64*1024)*1024;
		if(content_length > allowed) { 
			// TODO log
			return;
		}
		load_multipart_form_data();
		return;
	}

	long long allowed=service().settings().integer("security.content_length_limit",1024)*1024;
	if(content_length > allowed) {
		// TODO log
		return;
	}

	content_.clear();
	content_.resize(content_length,0);

	async_read(	&content_.front(),
			content_.size(),
			boost::bind(&connection::on_content_read,shared_from_this(),_1));

}

void connection::load_multipart_form_data()
{
	// TODO Load it
	context_.reset(new http::context(this));
	if(!context_->request().prepare())
		return;
	setup_application();
}

void connection::process_request(boost::system::error_code const &e)
{
	if(e) return;
	context_.reset(new http::context(this));
	context_->request().set_post_data(content_);
	if(!context_->request().prepare())
		return;
	setup_application();
}

void connection::make_error_response(int status,char const *msg)
{
	context_->response().io_mode(http::response::asynchronous);
	context_->response().status(status);
	context_->response().out() <<
		"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\n"
		"	\"http://www.w3.org/TR/html4/loose.dtd\">\n"
		"<html>\n"
		"  <head>\n"
		"    <title>"<<status<<" &emdash; "<< http::response::status_to_string(status)<<"</title>\n"
		"  </head>\n"
		"  <body>\n"
		"    <h1>"<<status<<" &emdash; "<< http::response::status_to_string(status)<<"</h1>\n"
		"    <p>"<<msg<<"</p>\n"
		"  </body>\n"
		"</html>\n"<<std::flush;
}


void connection::setup_application()
{
	std::string path = getenv("PATH_INFO");
	std::string matched;

	application_ = service().applications_pool().get(path,matched);

	url_dispatcher::dispatch_type how;
	if(application_.get() == 0 || (how=application_->dispatcher().dispatchable(path))!=url_dispatcher::none) {
		make_error_response(http::response::not_found);
		return;
	}

	if(how == url_dispatcher::asynchronous) {
		dispatch(false);
	}
	else {
		service().thread_pool().post(
			boost::bind(
				&connection::dispatch,
				shared_from_this(),
				true));
	}
}

void connection::dispatch(bool in_thread)
{
	try {
		application_->dispatcher().dispatch();
		context_->response().finalize();
	}
	catch(std::exception const &e){
		// TODO
	}
	if(in_thread)
		get_io_service().post(boost::bind(&connection::on_response_complete,shared_from_this()));
	else
		on_response_complete();
}

void connection::on_response_complete()
{
	service().applications_pool().put(application_);
	if(keep_alive()) {
		context_.reset();
		on_accepted();
	}
}

namespace {
	struct reader {
		reader(connection *C,io_handler const &H,size_t S,char *P) : h(H), s(S), p(P),conn(C)
		{
			done=0;
		}
		io_handler h;
		size_t s;
		size_t done;
		char *p;
		connection *conn;
		void operator() (boost::system::error_code const &e=boost::system::error_code(),size_t read = 0)
		{
			if(e) {
				h(e,done+read);
			}
			s-=read;
			p+=read;
			done+=read;
			if(s==0)
				h(boost::system::error_code(),done);
			else
				conn->async_read_some(p,s,*this);
		}
	};
	struct writer {
		writer(connection *C,io_handler const &H,size_t S,char const *P) : h(H), s(S), p(P),conn(C)
		{
			done=0;
		}
		io_handler h;
		size_t s;
		size_t done;
		char const *p;
		connection *conn;
		void operator() (boost::system::error_code const &e=boost::system::error_code(),size_t wr = 0)
		{
			if(e) {
				h(e,done+wr);
			}
			s-=wr;
			p+=wr;
			done+=wr;
			if(s==0)
				h(boost::system::error_code(),done);
			else
				conn->async_write_some(p,s,*this);
		}
	};
} // namespace

void connection::async_read(void *p,size_t s,io_handler const &h)
{
	reader r(this,h,s,(char*)p);
	r();
}

void connection::async_write(void const *p,size_t s,io_handler const &h)
{
	writer w(this,h,s,(char const *)p);
	w();
}


} // cgi
} // impl
} // cppcms
