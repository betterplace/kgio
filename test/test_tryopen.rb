require 'tempfile'
require 'test/unit'
$-w = true
require 'kgio'

class TestTryopen < Test::Unit::TestCase

  def test_tryopen_success
    tmp = Kgio.tryopen(__FILE__)
    assert_instance_of File, tmp
    assert_equal File.read(__FILE__), tmp.read
    assert_nothing_raised { tmp.close }
  end

  def test_tryopen_ENOENT
    tmp = Tempfile.new "tryopen"
    path = tmp.path
    tmp.close!
    tmp = Kgio.tryopen(path)
    assert_equal :ENOENT, tmp
  end

  def test_tryopen_EPERM
    tmp = Tempfile.new "tryopen"
    File.chmod 0000, tmp.path
    tmp = Kgio.tryopen(tmp.path)
    assert_equal :EACCES, tmp
  end

  def test_tryopen_readwrite
    tmp = Tempfile.new "tryopen"
    file = Kgio.tryopen(tmp.path, IO::RDWR)
    file.syswrite "FOO"
    assert_equal "FOO", tmp.sysread(3)
  end

  def test_tryopen_mode
    tmp = Tempfile.new "tryopen"
    path = tmp.path
    tmp.close!
    file = Kgio.tryopen(path, IO::RDWR|IO::CREAT, 0000)
    assert_equal 0100000, File.stat(path).mode
    ensure
      File.unlink path
  end

  require "benchmark"
  def test_benchmark
    nr = 1000000
    tmp = Tempfile.new('tryopen')
    file = tmp.path
    Benchmark.bmbm do |x|
      x.report("tryopen (OK)") do
        nr.times { Kgio.tryopen(file).close }
      end
      x.report("open (OK)") do
        nr.times { File.readable?(file) && File.open(file).close }
      end
    end
    tmp.close!
    assert_equal :ENOENT, Kgio.tryopen(file)
    Benchmark.bmbm do |x|
      x.report("tryopen (ENOENT)") do
        nr.times { Kgio.tryopen(file) }
      end
      x.report("open (ENOENT)") do
        nr.times { File.readable?(file) && File.open(file) }
      end
    end
  end if ENV["BENCHMARK"]
end
