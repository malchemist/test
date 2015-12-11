#include <iostream>
#include <fstream>
#include <thread>

#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/bind.hpp>
#include <boost/crc.hpp>

namespace ba = boost::asio;

struct handler
{
    ba::io_service::strand          &disp_read_;
    ba::io_service::strand          &disp_write_;
    std::shared_ptr<std::ifstream>   inp_;
    std::shared_ptr<std::ofstream>   out_;
    size_t                           block_size_;
    std::vector<char>                buffer_;

    handler( ba::io_service::strand &disp, ba::io_service::strand &dispw, std::shared_ptr<std::ifstream> inp, size_t block_size, std::shared_ptr<std::ofstream> out )
        : disp_read_( disp )
        , disp_write_( dispw )
        , inp_( inp )
        , out_( out )
        , block_size_( block_size )
        , buffer_( block_size )
    { }

    void calculate( ) {
        disp_read_.post( boost::bind( &handler::read, this ) );
    }

    void read( ) {
        auto curr_read_pos = inp_->tellg( );
        inp_->read( buffer_.data( ), buffer_.size( ) );
        if( inp_->gcount( ) != 0 ) {
            auto write_pos = curr_read_pos / buffer_.size( ) * sizeof(boost::crc_32_type::value_type);
            disp_read_.get_io_service( ).post( std::bind( &handler::calc, this, inp_->gcount( ), write_pos ) );
        }
    }

    void calc(size_t read_size, size_t write_pos) {
        boost::crc_32_type crc;
        crc.process_bytes( buffer_.data(), read_size );
        auto sign = crc.checksum( );

        disp_write_.post( std::bind( &handler::write, this, sign, write_pos ) );
        disp_read_.post( std::bind( &handler::read, this ) );
    }

    void write( boost::crc_32_type::value_type sign, size_t write_pos ) {
        out_->seekp( write_pos );
        out_->write( reinterpret_cast<const char *>( &sign ), sizeof( sign ) );
    }
};

void attach( handler &hdl ) {
    try {
        hdl.calculate( );
        hdl.disp_read_.get_io_service( ).run( );
    } catch ( const std::exception &ex ) {
        std::cout << "Execution failed: " << ex.what( ) << std::endl;
        hdl.disp_read_.get_io_service( ).stop( );
    }
}

int main( int argc, const char *argv[] )
{
    if ( argc < 3 ) {
        std::cout << "Found " << argc - 1 << " argument(s)\n";
        std::cout << "Need 3 arguments...\n";
        return 1;
    }

    const std::string input_file  = argv[1];
    const std::string output_file = argv[2];

    size_t block_size = 1024 << 10;
    if ( argc == 4 ) {
        try {
            std::string s = argv[3];
            block_size = std::stoull( s );
        } catch( const std::exception &ex ) {
            std::cout << "Invalid block size: " << ex.what( );
            return 1;
        }
    }

    auto threads_num = std::thread::hardware_concurrency( );
    threads_num = !threads_num ? 2 : threads_num;

    try {
        ba::io_service          ios;
        ba::io_service::strand  disp_read( ios );
        ba::io_service::strand  disp_write( ios);

        auto inp_sptr = std::make_shared<std::ifstream>( input_file,  std::ios::binary | std::ios::in );
        if ( inp_sptr->fail( ) ) {
            std::cout << "Cannot open input file\n";
            return 1;
        }
        auto out_sptr = std::make_shared<std::ofstream>( output_file, std::ios::binary | std::ios::out );
        if ( out_sptr->fail( ) ) {
            std::cout << "Cannot open otuput file\n";
            return 1;
        }

        std::vector<handler> handlers;
        for ( size_t i = 0; i < threads_num; ++i ) {
            handlers.emplace_back( disp_read, disp_write, inp_sptr, block_size, out_sptr );
        }

        std::vector<std::thread> threads;
        for ( size_t i = 1; i < threads_num; ++i ) {
            threads.emplace_back( attach, std::ref( handlers[i] ) );
        }
        attach( handlers[0] );

        for ( auto &thread: threads) {
            if ( thread.joinable( ) ) {
                thread.join( );
            }
        }
    } catch( const std::exception &ex ) {
        std::cerr << "Failed: " << ex.what( );
    }

    return 0;
}