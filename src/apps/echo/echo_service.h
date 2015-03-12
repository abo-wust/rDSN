/*
 * The MIT License (MIT)

 * Copyright (c) 2015 Microsoft Corporation

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
# pragma once

# include <dsn/serviceletex.h>
# include <iostream>

DEFINE_THREAD_POOL_CODE(THREAD_POOL_TEST)
DEFINE_TASK_CODE(LPC_ECHO_TIMER, ::dsn::TASK_PRIORITY_HIGH, THREAD_POOL_TEST)
DEFINE_TASK_CODE_RPC(RPC_ECHO, ::dsn::TASK_PRIORITY_HIGH, THREAD_POOL_TEST)
DEFINE_TASK_CODE_RPC(RPC_ECHO2, ::dsn::TASK_PRIORITY_HIGH, THREAD_POOL_TEST)

using namespace dsn;
using namespace dsn::service;

class echo_server : public serviceletex<echo_server>
{
public:
    echo_server() : serviceletex<echo_server>("echo_server")
    {
        register_rpc_handler(RPC_ECHO, "RPC_ECHO", &echo_server::on_echo);
        register_async_rpc_handler(RPC_ECHO2, "RPC_ECHO2", &echo_server::on_echo2);
    }

    void on_echo(const std::string& req, __out_param std::string& resp)
    {
        resp = req;
    }

    void on_echo2(const std::string& req, rpc_replier<std::string>& reply)
    {
        reply(req);
    }
};

class echo_server_app : public service_app
{
public:
    echo_server_app(service_app_spec* s, configuration_ptr c)
        : service_app(s, c)
    {
        _server = nullptr;
    }

    virtual ~echo_server_app(void)
    {
    }

    virtual error_code start(int argc, char** argv)
    {
        _server = new echo_server();
        return ERR_SUCCESS;
    }

    virtual void stop(bool cleanup = false)
    {
        delete _server;
        _server = nullptr;
    }

private:
    echo_server *_server;
};

class echo_client : public serviceletex<echo_client>
{
public:
    echo_client(const char* host, uint16_t port, 
        int message_size,
        int concurrency = 1
    )
        : serviceletex<echo_client>("echo_client")
    {
        _seq = 0;
        _message_size = message_size;
        _concurrency = concurrency;
        _server = end_point(host, port);

        enqueue_task(LPC_ECHO_TIMER, &echo_client::on_echo_timer, 0, 0, 1000);
    }

    void on_echo_timer()
    {
        for (int i = 0; i < _concurrency; i++)
        {
            char buf[120];
            sprintf(buf, "%u", ++_seq);
            std::shared_ptr<std::string> req(new std::string("hi, dsn "));
            *req = req->append(buf);
            req->resize(_message_size);
            rpc_typed(_server, RPC_ECHO, req, &echo_client::on_echo_reply, 0, 3000);
        }

        std::cout
            << "echo: " << _seq
            << ", throughput(MB/s) = "
            << ((double)_message_size * (double)_concurrency / 1024.0 / 1024.0)
            << std::endl;
    }

    void on_echo_reply(error_code err, std::shared_ptr<std::string> req, std::shared_ptr<std::string> resp)
    {
        if (err != ERR_SUCCESS) std::cout << "echo err: " << err.to_string() << std::endl;
        else
        {
            //std::cout << "echo result: " << resp->c_str() << "(len = " << resp->length() << ")" << std::endl;
        }
    }

private:
    end_point _server;
    int _seq;
    int _message_size;
    int _concurrency;
};

class echo_client_app : public service_app
{
public:
    echo_client_app(service_app_spec* s, configuration_ptr c)
        : service_app(s, c)
    {
        _client = nullptr;
    }

    virtual ~echo_client_app(void)
    {
    }

    virtual error_code start(int argc, char** argv)
    {
        if (argc < 3)
            return ERR_INVALID_PARAMETERS;

        int sz = config()->get_value<int>("apps.client", "message_size", 1024);
        int cc = config()->get_value<int>("apps.client", "concurrency", 1);
        _client = new echo_client(argv[1], (uint16_t)atoi(argv[2]), sz, cc);
        return ERR_SUCCESS;
    }

    virtual void stop(bool cleanup = false)
    {
        delete _client;
        _client = nullptr;
    }

private:
    echo_client *_client;
};
