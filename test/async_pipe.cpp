// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#define BOOST_TEST_MAIN


#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/config.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/process.hpp>
#include <boost/process/async_pipe.hpp>
#include <boost/process/pipe.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace std;
namespace bp = boost::process;
namespace asio = boost::asio;
namespace bfs = boost::filesystem;

BOOST_AUTO_TEST_SUITE( async );


BOOST_AUTO_TEST_CASE(plain_async, *boost::unit_test::timeout(5))
{
    asio::io_context ios;
    bp::async_pipe pipe{ios};

    std::string st = "test-string\n";

    asio::streambuf buf;

    asio::async_write(pipe, asio::buffer(st), [](const boost::system::error_code &, std::size_t){});
    asio::async_read_until(pipe, buf, '\n', [](const boost::system::error_code &, std::size_t){});

    ios.run();

    std::string line;
    std::istream istr(&buf);
    BOOST_CHECK(std::getline(istr, line));

    line.resize(11);
    BOOST_CHECK_EQUAL(line, "test-string");

}

BOOST_AUTO_TEST_CASE(closed_transform)
{
    asio::io_context ios;

    bp::async_pipe ap{ios};

    BOOST_CHECK(ap.is_open());
    bp::pipe p2 = static_cast<bp::pipe>(ap);
    BOOST_CHECK(p2.is_open());

    ap.close();
    BOOST_CHECK(!ap.is_open());

    bp::pipe p  = static_cast<bp::pipe>(ap);
    BOOST_CHECK(!p.is_open());

}

BOOST_AUTO_TEST_CASE(multithreaded_async_pipe)
{
    asio::io_context ioc;

    std::vector<std::thread> threads;
    for (int i = 0; i < std::thread::hardware_concurrency(); i++)
    {
        threads.emplace_back([&ioc]
        {
            std::vector<bp::async_pipe*> pipes;
            for (size_t i = 0; i < 100; i++)
                pipes.push_back(new bp::async_pipe(ioc));
            for (auto &p : pipes)
                delete p;
        });
    }
    for (auto &t : threads)
        t.join();
}

#define log_stmt(stmt) \
    BOOST_TEST_MESSAGE(__LINE__ << ": " << #stmt); \
    stmt; \
    BOOST_TEST_MESSAGE(__LINE__ << ": done")

namespace
{
struct named_pipe_test_fixture
{
    using async_pipe_ptr = std::shared_ptr<bp::async_pipe>;

    asio::io_context ioc;

    boost::uuids::random_generator uuidGenerator;
    const bfs::path pipe_path;
    const std::string pipe_name;

    async_pipe_ptr created_pipe;
    async_pipe_ptr opened_pipe;

    const char delim;
    const std::string st_base;
    const std::string st;

    asio::streambuf buf;

    named_pipe_test_fixture()
    : ioc{}
    // generate a unique random path/name for for the pipe
    , uuidGenerator{}
    , pipe_path{
        #if defined(BOOST_POSIX_API)
            bfs::temp_directory_path()
        #elif defined(BOOST_WINDOWS_API)
            bfs::path("\\\\.\\pipe")
        #endif
        / boost::uuids::to_string(uuidGenerator())
      }
    , pipe_name{pipe_path.string()}
    , delim{'\n'}
    , st_base{"test-string"}
    , st{st_base + delim}
    , buf{}
    {
        // create and open the pipe "file"
        created_pipe.reset(new bp::async_pipe(ioc, pipe_name));
        BOOST_CHECK(created_pipe->is_open());

        // open the existing pipe
        opened_pipe.reset(new bp::async_pipe(ioc, pipe_name, true));
        BOOST_CHECK(opened_pipe->is_open());
    }

    ~named_pipe_test_fixture()
    {
        std::string line;
        std::istream istr(&buf);
        BOOST_CHECK(std::getline(istr, line));

        BOOST_CHECK_EQUAL(line, st_base);

        // close pipes
        created_pipe->close();
        BOOST_CHECK(!created_pipe->is_open());
        opened_pipe->close();
        BOOST_CHECK(!opened_pipe->is_open());
        // cleanup
        bfs::remove(pipe_path);
        BOOST_CHECK(!bfs::exists(pipe_path));
    }



    static
    void test_plain_async(asio::io_context& ioc,
                          async_pipe_ptr async_writer, async_pipe_ptr async_reader,
                          const std::string& st, asio::streambuf& buf, const char delim)
    {
        log_stmt(
            asio::async_write(*async_writer, asio::buffer(st),
                [](const boost::system::error_code &, std::size_t){
                    BOOST_TEST_MESSAGE("        in async_write");
                })
        );
        log_stmt(
            asio::async_read_until(*async_reader, buf, delim,
                [](const boost::system::error_code &, std::size_t){
                    BOOST_TEST_MESSAGE("        in async_read_until");
                })
        );
        log_stmt(ioc.run());
    }

};

}

BOOST_FIXTURE_TEST_SUITE(existing_named_pipe_plain_async, named_pipe_test_fixture)

BOOST_AUTO_TEST_CASE(existing_named_pipe_plain_async_created_pipe, *boost::unit_test::timeout(5))
{
    test_plain_async(ioc, created_pipe, created_pipe, st, buf, delim);
}

BOOST_AUTO_TEST_CASE(existing_named_pipe_plain_async_opened_pipe, *boost::unit_test::timeout(5))
{
    test_plain_async(ioc, opened_pipe, opened_pipe, st, buf, delim);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(existing_named_pipe_send_receive_child_process, *boost::unit_test::timeout(5))
{
    namespace bp = boost::process;
    using std::string;

    asio::io_context ioc;
    // generate a unique random path/name for for the pipe
    boost::uuids::random_generator uuidGenerator{};
    const bfs::path pipe_path_base =
        #if defined(BOOST_POSIX_API)
            bfs::temp_directory_path();
        #elif defined(BOOST_WINDOWS_API)
            bfs::path("\\\\.\\pipe");
        #endif
    const string pipe_name_base = (pipe_path_base / boost::uuids::to_string(uuidGenerator())).string();
    const char delim     = '\n';

    const string st_base = "test-string";
    const string st      = st_base + delim;

    asio::streambuf buf{};

    // create and open the pipe "file"
    const string out_pipe_name = pipe_name_base + "-1";
    bp::async_pipe out_pipe(ioc, out_pipe_name);
    BOOST_CHECK(out_pipe.is_open());

    // open the existing pipe
    const string in_pipe_name = pipe_name_base + "-0";
    bp::async_pipe in_pipe(ioc, in_pipe_name);
    BOOST_CHECK(in_pipe.is_open());

    BOOST_TEST_MESSAGE("        out_pipe_name [" << out_pipe_name << "]");
    BOOST_TEST_MESSAGE("        in_pipe_name  [" << in_pipe_name << "]");

    const string child_path = (
        boost::filesystem::system_complete(
            boost::unit_test::framework::master_test_suite().argv[0]
        ).remove_filename() /
        #if defined(BOOST_POSIX_API)
            "echo_named_pipe"
        #elif defined(BOOST_WINDOWS_API)
            "echo_named_pipe.exe"
        #endif
    ).string();
    BOOST_TEST_MESSAGE("        starting child [" << child_path << "]");
    log_stmt(
        bp::child c(
            bp::exe=child_path,
            bp::args={
                "--input", out_pipe_name,
                "--output", in_pipe_name
            },
            bp::std_out > stdout,
            bp::std_err > stderr
        )
    );



    log_stmt(
        asio::async_write(out_pipe, asio::buffer(st), [&](const boost::system::error_code &, std::size_t){
            BOOST_TEST_MESSAGE("        in async_write");
        });
    );
    log_stmt(
        asio::async_read_until(in_pipe, buf, delim, [&](const boost::system::error_code &, std::size_t){
            BOOST_TEST_MESSAGE("        in async_read_until");
        });
    );


    log_stmt(ioc.run());

    log_stmt(c.wait());




    string line;
    std::istream istr(&buf);
    BOOST_CHECK(std::getline(istr, line));

    BOOST_CHECK_EQUAL(line, st_base);

    // close pipes
    out_pipe.close();
    BOOST_CHECK(!out_pipe.is_open());
    in_pipe.close();
    BOOST_CHECK(!in_pipe.is_open());
    // cleanup
    bfs::remove(in_pipe_name);
    BOOST_CHECK(!bfs::exists(in_pipe_name));
    bfs::remove(out_pipe_name);
    BOOST_CHECK(!bfs::exists(out_pipe_name));
}



BOOST_AUTO_TEST_CASE(move_pipe)
{
    asio::io_context ios;

    bp::async_pipe ap{ios};
    BOOST_TEST_CHECKPOINT("First move");
    bp::async_pipe ap2{std::move(ap)};
    BOOST_CHECK_EQUAL(ap.native_source(), ::boost::winapi::INVALID_HANDLE_VALUE_);
    BOOST_CHECK_EQUAL(ap.native_sink  (), ::boost::winapi::INVALID_HANDLE_VALUE_);

    BOOST_TEST_CHECKPOINT("Second move");
    ap = std::move(ap2);

    {
        BOOST_TEST_CHECKPOINT("Third move, from closed");
        bp::async_pipe ap_inv{ios};
        ap_inv.close();
        ap = std::move(ap_inv);
    }

    {
        BOOST_TEST_CHECKPOINT("Fourth move, from closed");
        bp::async_pipe ap_inv{ios};
        ap_inv.close();
        const auto ap3 = std::move(ap_inv);
    }

    {
        //copy an a closed pipe
        BOOST_TEST_CHECKPOINT("Copy assign");
        BOOST_TEST_CHECKPOINT("Fourth move, from closed");
        bp::async_pipe ap_inv{ios};
        ap_inv.close();
        ap = ap_inv; //copy an invalid pipe
    }

    {
        //copy an a closed pipe
        BOOST_TEST_CHECKPOINT("Copy assign");
        BOOST_TEST_CHECKPOINT("Fourth move, from closed");
        bp::async_pipe ap_inv{ios};
        ap_inv.close();
        BOOST_TEST_CHECKPOINT("Copy construct");
        bp::async_pipe ap4{ap_inv};
    }


}


BOOST_AUTO_TEST_SUITE_END();
