// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>

#include <boost/process.hpp>
#include <boost/process/posix.hpp>

#include <system_error>


#include <string>
#include <thread>
#include <vector>
#include <sys/wait.h>
#include <errno.h>

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(bind_fd, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::pipe p;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test", "--posix-echo-one", "3", "hello",
        bp::posix::fd.bind(3, p.native_sink()),
        ec
    );
    BOOST_CHECK(!ec);


    bp::ipstream is(std::move(p));

    std::string s;
    is >> s;
    BOOST_CHECK_EQUAL(s, "hello");
}

BOOST_AUTO_TEST_CASE(bind_fds, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::pipe p1;
    bp::pipe p2;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test","--posix-echo-two","3","hello","99","bye",
        bp::posix::fd.bind(3,  p1.native_sink()),
        bp::posix::fd.bind(99, p2.native_sink()),
        ec
    );
    BOOST_CHECK(!ec);

    bp::ipstream is1(std::move(p1));
    bp::ipstream is2(std::move(p2));

    std::string s1;
    is1 >> s1;
    BOOST_CHECK_EQUAL(s1, "hello");

    std::string s2;
    is2 >> s2;
    BOOST_CHECK_EQUAL(s2, "bye");
}

BOOST_AUTO_TEST_CASE(execve_set_on_error, *boost::unit_test::timeout(2))
{
    std::error_code ec;
    bp::spawn(
        "doesnt-exist",
        ec
    );
    BOOST_CHECK(ec);
    BOOST_CHECK_EQUAL(ec.value(), ENOENT);
}

BOOST_AUTO_TEST_CASE(execve_throw_on_error, *boost::unit_test::timeout(2))
{
    try
    {
        bp::spawn("doesnt-exist");
        BOOST_CHECK(false);
    }
    catch (bp::process_error &e)
    {
        BOOST_CHECK(e.code());
        BOOST_CHECK_EQUAL(e.code().value(), ENOENT);
    }
}

BOOST_AUTO_TEST_CASE(handle_inheritance_singlethreaded, *boost::unit_test::timeout(5)) {
    using boost::unit_test::framework::master_test_suite;

    boost::process::pipe p1;
    boost::process::pipe p2;
    boost::process::child c1(
        master_test_suite().argv[1], "--stdin-to-stdout",
        boost::process::std_in < p1);
    boost::process::child c2(
        master_test_suite().argv[1], "--stdin-to-stdout",
        boost::process::std_in < p2);

    p1.close();
    p2.close();

    c1.wait(); // blocks until the input is properly closed
    c2.wait(); // blocks until the input is properly closed
}

static void run_cat() {
    using boost::unit_test::framework::master_test_suite;

    boost::process::pipe p;
    boost::process::child c(
        master_test_suite().argv[1], "--stdin-to-stdout",
        boost::process::std_in < p);

    p.close();

    c.wait(); // blocks until the input is properly closed
}

BOOST_AUTO_TEST_CASE(handle_inheritance_multithreaded, *boost::unit_test::timeout(20)) {
    int nThreads = 1000;

    std::vector<std::thread> threads;
    threads.reserve(nThreads);

    for (int i = 0; i < nThreads; ++i) {
        threads.emplace_back(run_cat);
    }

    for (int i = 0; i < nThreads; ++i) {
        if (threads[i].joinable()) {
            threads[i].join();
        }
    }
}
