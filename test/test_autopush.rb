require 'tempfile'
require 'test/unit'
RUBY_PLATFORM =~ /linux/ and require 'strace'
$-w = true
require 'kgio'

class TestAutopush < Test::Unit::TestCase
  TCP_CORK = 3

  def setup
    Kgio.autopush = false
    assert_equal false, Kgio.autopush?

    @host = ENV["TEST_HOST"] || '127.0.0.1'
    @srv = Kgio::TCPServer.new(@host, 0)
    assert_nothing_raised {
      @srv.setsockopt(Socket::SOL_TCP, TCP_CORK, 1)
    } if RUBY_PLATFORM =~ /linux/
    @port = @srv.addr[1]
  end

  def test_autopush_true_unix
    Kgio.autopush = true
    tmp = Tempfile.new('kgio_unix')
    @path = tmp.path
    File.unlink(@path)
    tmp.close rescue nil
    @srv = Kgio::UNIXServer.new(@path)
    @rd = Kgio::UNIXSocket.new(@path)
    io, err = Strace.me { @wr = @srv.kgio_accept }
    assert_nil err
    rc = nil
    io, err = Strace.me {
      @wr.kgio_write "HI\n"
      rc = @wr.kgio_tryread 666
    }
    assert_nil err
    lines = io.readlines
    assert lines.grep(/TCP_CORK/).empty?, lines.inspect
    assert_equal :wait_readable, rc
  ensure
    File.unlink(@path) rescue nil
  end

  def test_autopush_false
    Kgio.autopush = nil
    assert_equal false, Kgio.autopush?

    @wr = Kgio::TCPSocket.new(@host, @port)
    io, err = Strace.me { @rd = @srv.kgio_accept }
    assert_nil err
    lines = io.readlines
    assert lines.grep(/TCP_CORK/).empty?, lines.inspect
    assert_equal 1, @rd.getsockopt(Socket::SOL_TCP, TCP_CORK).unpack("i")[0]

    rbuf = "..."
    t0 = Time.now
    @rd.kgio_write "HI\n"
    @wr.kgio_read(3, rbuf)
    diff = Time.now - t0
    assert(diff >= 0.200, "TCP_CORK broken? diff=#{diff} > 200ms")
    assert_equal "HI\n", rbuf
  end if RUBY_PLATFORM =~ /linux/

  def test_autopush_true
    Kgio.autopush = true
    assert_equal true, Kgio.autopush?
    @wr = Kgio::TCPSocket.new(@host, @port)
    io, err = Strace.me { @rd = @srv.kgio_accept }
    assert_nil err
    lines = io.readlines
    assert_equal 1, lines.grep(/TCP_CORK/).size, lines.inspect
    assert_equal 1, @rd.getsockopt(Socket::SOL_TCP, TCP_CORK).unpack("i")[0]

    @wr.write "HI\n"
    rbuf = ""
    io, err = Strace.me { @rd.kgio_read(3, rbuf) }
    assert_nil err
    lines = io.readlines
    assert lines.grep(/TCP_CORK/).empty?, lines.inspect
    assert_equal "HI\n", rbuf

    t0 = Time.now
    @rd.kgio_write "HI2U2\n"
    @rd.kgio_write "HOW\n"
    rc = false
    io, err = Strace.me { rc = @rd.kgio_tryread(666) }
    @wr.readpartial(666, rbuf)
    rbuf == "HI2U2\nHOW\n" or warn "rbuf=#{rbuf.inspect} looking bad?"
    diff = Time.now - t0
    assert(diff < 0.200, "time diff=#{diff} >= 200ms")
    assert_equal :wait_readable, rc
    assert_nil err
    lines = io.readlines
    assert_equal 2, lines.grep(/TCP_CORK/).size, lines.inspect
    assert_nothing_raised { @wr.close }
    assert_nothing_raised { @rd.close }

    @wr = Kgio::TCPSocket.new(@host, @port)
    io, err = Strace.me { @rd = @srv.kgio_accept }
    assert_nil err
    lines = io.readlines
    assert lines.grep(/TCP_CORK/).empty?, "optimization fail: #{lines.inspect}"
    assert_equal 1, @rd.getsockopt(Socket::SOL_TCP, TCP_CORK).unpack("i")[0]
  end if RUBY_PLATFORM =~ /linux/

  def teardown
    Kgio.autopush = false
  end
end
