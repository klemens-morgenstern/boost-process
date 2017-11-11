// Copyright (c) 2015 Klemens D. Morgenstern
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/program_options.hpp>

#include <vector>
#include <string>

#include <iostream>
#include <cstdint>

#include <fstream>

#include <chrono>


int main(int argc, char *argv[])
{
    using namespace std;
    using namespace boost::program_options;
    using namespace boost::process;
    namespace asio = boost::asio;

    cout << "child: started\n";

    options_description desc;
    desc.add_options()
         ("input", value<string>())
         ("output", value<string>())
         ;

    variables_map vm;
    command_line_parser parser(argc, argv);
    store(parser.options(desc).allow_unregistered().run(), vm);
    notify(vm);

    asio::io_context ioc;

    const string input_pipe_name  = vm["input"].as<string>();
    const string output_pipe_name = vm["output"].as<string>();

    async_pipe input_pipe(ioc, input_pipe_name, true);
    if (!input_pipe.is_open())
    {
        throw runtime_error(string("child: failed to open the input pipe [") + input_pipe_name + "]");
    }
    cout << "child: input: [" << input_pipe_name << "]\n";

    async_pipe output_pipe(ioc, output_pipe_name, true);
    if (!output_pipe.is_open())
    {
        throw runtime_error(string("child: failed to open the output pipe [") + output_pipe_name + "]");
    }
    cout << "child: output: [" << output_pipe_name << "]\n";


    const char delim = '\n';
    boost::asio::streambuf in_buf;
    asio::async_read_until(input_pipe, in_buf, delim, [&](const boost::system::error_code &, std::size_t){

        string out_buf;
        std::istream istr(&in_buf);
        std::getline(istr, out_buf, delim);
        out_buf += delim;

        cout << "child: received [" << out_buf << "]\n";

        asio::async_write(output_pipe, asio::buffer(out_buf), [&](const boost::system::error_code &, std::size_t){
            cout << "child: sent [" << out_buf << "]\n";
        });
    });

    ioc.run();

    cout << "child: done\n";
    return 0;
}
