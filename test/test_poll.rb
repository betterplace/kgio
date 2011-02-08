require 'test/unit'
$-w = true
require 'kgio'

class TestPoll < Test::Unit::TestCase
  def teardown
    [ @rd, @wr ].each { |io| io.close unless io.closed? }
  end

  def setup
    @rd, @wr = IO.pipe
  end

  def test_constants
    assert_kind_of Integer, Kgio::POLLIN
    assert_kind_of Integer, Kgio::POLLOUT
    assert_kind_of Integer, Kgio::POLLPRI
    assert_kind_of Integer, Kgio::POLLHUP
    assert_kind_of Integer, Kgio::POLLERR
    assert_kind_of Integer, Kgio::POLLNVAL
  end

  def test_poll_symbol
    set = { @rd => :wait_readable, @wr => :wait_writable }
    res = Kgio.poll(set)
    assert_equal({@wr => Kgio::POLLOUT}, res)
    assert_equal set.object_id, res.object_id
  end

  def test_poll_integer
    set = { @wr => Kgio::POLLOUT|Kgio::POLLHUP }
    res = Kgio.poll(set)
    assert_equal({@wr => Kgio::POLLOUT}, res)
    assert_equal set.object_id, res.object_id
  end

  def test_poll_timeout
    t0 = Time.now
    res = Kgio.poll({}, 10)
    diff = Time.now - t0
    assert diff >= 0.010, "diff=#{diff}"
    assert_nil res
  end

  def test_poll_interrupt
    foo = nil
    oldquit = trap(:QUIT) { foo = :bar }
    thr = Thread.new { sleep 0.100; Process.kill(:QUIT, $$) }
    t0 = Time.now
    assert_raises(Errno::EINTR) { Kgio.poll({}) }
    diff = Time.now - t0
    thr.join
    assert diff >= 0.010, "diff=#{diff}"
    ensure
      trap(:QUIT, "DEFAULT")
  end

  def test_poll_close
    foo = nil
    thr = Thread.new { sleep 0.100; @wr.close }
    t0 = Time.now
    res = Kgio.poll({@rd => Kgio::POLLIN})
    diff = Time.now - t0
    thr.join
    assert_equal([ @rd ], res.keys)
    assert diff >= 0.010, "diff=#{diff}"
  end
end if Kgio.respond_to?(:poll)
