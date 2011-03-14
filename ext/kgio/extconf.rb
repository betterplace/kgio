require 'mkmf'
$CPPFLAGS << ' -D_GNU_SOURCE'
$CPPFLAGS << ' -DPOSIX_C_SOURCE=1'

have_func("poll", "poll.h")
have_func("getaddrinfo", %w(sys/types.h sys/socket.h netdb.h)) or
  abort "getaddrinfo required"
have_func("getnameinfo", %w(sys/types.h sys/socket.h netdb.h)) or
  abort "getnameinfo required"
have_type("struct sockaddr_storage", %w(sys/types.h sys/socket.h)) or
  abort "struct sockaddr_storage required"
have_func('accept4', %w(sys/socket.h))
if have_header('ruby/io.h')
  rubyio = %w(ruby.h ruby/io.h)
  have_struct_member("rb_io_t", "fd", rubyio)
  have_struct_member("rb_io_t", "mode", rubyio)
else
  rubyio = %w(ruby.h rubyio.h)
  rb_io_t = have_type("OpenFile", rubyio) ? "OpenFile" : "rb_io_t"
  have_struct_member(rb_io_t, "f", rubyio)
  have_struct_member(rb_io_t, "f2", rubyio)
  have_struct_member(rb_io_t, "mode", rubyio)
  have_func('rb_fdopen')
end
have_type("struct RFile", rubyio) and check_sizeof("struct RFile", rubyio)
have_type("struct RObject") and check_sizeof("struct RObject")
check_sizeof("int")
have_func('rb_io_ascii8bit_binmode')
have_func('rb_thread_blocking_region')
have_func('rb_thread_io_blocking_region')
have_func('rb_str_set_len')

dir_config('kgio')
create_makefile('kgio_ext')
