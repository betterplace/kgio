# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name = "kgio"
  s.version = "2.8.1"

  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
  s.authors = ["kgio hackers"]
  s.date = "2014-02-03"
  s.description = "Library for kinder, gentler I/O for Ruby"
  s.email = "kgio@librelist.org"
  s.extensions = ["ext/kgio/extconf.rb"]
  s.extra_rdoc_files = ["README", "lib/kgio.rb", "lib/kgio/version.rb", "ext/kgio/accept.c", "ext/kgio/autopush.c", "ext/kgio/connect.c", "ext/kgio/kgio_ext.c", "ext/kgio/poll.c", "ext/kgio/read_write.c", "ext/kgio/tryopen.c", "ext/kgio/wait.c"]
  s.files = [".document", ".gitignore", ".wrongdoc.yml", "COPYING", "GIT-VERSION-GEN", "GNUmakefile", "Gemfile", "HACKING", "ISSUES", "LICENSE", "README", "Rakefile", "TODO", "ext/kgio/accept.c", "ext/kgio/ancient_ruby.h", "ext/kgio/autopush.c", "ext/kgio/blocking_io_region.h", "ext/kgio/broken_system_compat.h", "ext/kgio/connect.c", "ext/kgio/extconf.rb", "ext/kgio/kgio.h", "ext/kgio/kgio_ext.c", "ext/kgio/missing_accept4.h", "ext/kgio/my_fileno.h", "ext/kgio/nonblock.h", "ext/kgio/poll.c", "ext/kgio/read_write.c", "ext/kgio/set_file_path.h", "ext/kgio/sock_for_fd.h", "ext/kgio/tryopen.c", "ext/kgio/wait.c", "kgio.gemspec", "lib/kgio.rb", "lib/kgio/version.rb", "pkg.mk", "setup.rb", "test/lib_read_write.rb", "test/lib_server_accept.rb", "test/test_accept_class.rb", "test/test_accept_flags.rb", "test/test_autopush.rb", "test/test_connect_fd_leak.rb", "test/test_cross_thread_close.rb", "test/test_default_wait.rb", "test/test_kgio_addr.rb", "test/test_no_dns_on_tcp_connect.rb", "test/test_peek.rb", "test/test_pipe_popen.rb", "test/test_pipe_read_write.rb", "test/test_poll.rb", "test/test_singleton_read_write.rb", "test/test_socket.rb", "test/test_socketpair_read_write.rb", "test/test_tcp6_client_read_server_write.rb", "test/test_tcp_client_read_server_write.rb", "test/test_tcp_connect.rb", "test/test_tcp_server.rb", "test/test_tcp_server_read_client_write.rb", "test/test_tfo.rb", "test/test_tryopen.rb", "test/test_unix_client_read_server_write.rb", "test/test_unix_connect.rb", "test/test_unix_server.rb", "test/test_unix_server_read_client_write.rb"]
  s.homepage = "http://bogomips.org/kgio/"
  s.rdoc_options = ["--title", "Kgio - kinder, gentler I/O for Ruby", "--main", "README"]
  s.require_paths = ["lib", "ext/kgio"]
  s.rubygems_version = "1.8.24"
  s.summary = "kinder, gentler I/O for Ruby"
  s.test_files = ["test/lib_read_write.rb", "test/lib_server_accept.rb", "test/test_accept_class.rb", "test/test_accept_flags.rb", "test/test_autopush.rb", "test/test_connect_fd_leak.rb", "test/test_cross_thread_close.rb", "test/test_default_wait.rb", "test/test_kgio_addr.rb", "test/test_no_dns_on_tcp_connect.rb", "test/test_peek.rb", "test/test_pipe_popen.rb", "test/test_pipe_read_write.rb", "test/test_poll.rb", "test/test_singleton_read_write.rb", "test/test_socket.rb", "test/test_socketpair_read_write.rb", "test/test_tcp6_client_read_server_write.rb", "test/test_tcp_client_read_server_write.rb", "test/test_tcp_connect.rb", "test/test_tcp_server.rb", "test/test_tcp_server_read_client_write.rb", "test/test_tfo.rb", "test/test_tryopen.rb", "test/test_unix_client_read_server_write.rb", "test/test_unix_connect.rb", "test/test_unix_server.rb", "test/test_unix_server_read_client_write.rb"]

  if s.respond_to? :specification_version then
    s.specification_version = 3

    if Gem::Version.new(Gem::VERSION) >= Gem::Version.new('1.2.0') then
      s.add_development_dependency(%q<gem_hadar>, ["~> 0.3.2"])
    else
      s.add_dependency(%q<gem_hadar>, ["~> 0.3.2"])
    end
  else
    s.add_dependency(%q<gem_hadar>, ["~> 0.3.2"])
  end
end
