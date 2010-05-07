///////////////////////////////////////////////////////////////////////////////
//                                                                             
//  Copyright (C) 2008-2010  Artyom Beilis (Tonkikh) <artyomtnk@yahoo.com>     
//                                                                             
//  This program is free software: you can redistribute it and/or modify       
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCMS_SERVICE_IMPL_H
#define CPPCMS_SERVICE_IMPL_H

#include "json.h"
#include "localization.h"
#include <booster/aio/io_service.h>
#include <memory>

#include "config.h"
#ifdef CPPCMS_USE_EXTERNAL_BOOST
#   include <boost/shared_ptr.hpp>
#else // Internal Boost
#   include <cppcms_boost/shared_ptr.hpp>
    namespace boost = cppcms_boost;
#endif


namespace cppcms {
class service;
class applications_pool;
class thread_pool;
class session_pool;

namespace impl {
	namespace cgi {
		class acceptor;
	}

	class service : public booster::noncopyable {
	public:
		service();
		~service();
		booster::aio::io_service &get_io_service()
		{
			return *io_service_;
		}


	private:
		friend class cppcms::service;
		std::auto_ptr<booster::aio::io_service> io_service_;

		std::vector<boost::shared_ptr<cgi::acceptor> > acceptors_;
		std::auto_ptr<json::value> settings_;
		std::auto_ptr<applications_pool> applications_pool_;
		std::auto_ptr<thread_pool> thread_pool_;
		std::auto_ptr<locale::generator> locale_generator_;
		std::auto_ptr<views_pool> views_pool_;
		std::auto_ptr<cache_pool> cache_pool_;
		std::auto_ptr<session_pool> session_pool_;
		std::locale default_locale_;

		std::vector<booster::function<void()> > on_fork_;

		int id_;

		booster::aio::native_type notification_socket_;
		std::auto_ptr<booster::aio::socket> sig_,breaker_;


	};


}
} // cppcms



#endif
