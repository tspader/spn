local spn = require('spn')

local recipe = spn.recipes.basic({
  git = 'curl/curl',
  libs = { 'curl' },
  kinds = { 'shared', 'static' },
  build = function(builder)
    local cmake = {
      defines = {
        { 'BUILD_TESTING', false },
        { 'CURL_DISABLE_TESTS', true },
        { 'CURL_DISABLE_EXAMPLES', true },
      },
      install = true
    }

    if builder.kind == spn.build_kind.static then
      table.insert(cmake.defines, { 'CURL_USE_OPENSSL', true })
      table.insert(cmake.defines, { 'CURL_BROTLI', false })
      table.insert(cmake.defines, { 'CURL_ZSTD', false })
      table.insert(cmake.defines, { 'CURL_USE_LIBPSL', false })
      table.insert(cmake.defines, { 'CURL_USE_LIBSSH2', false })
      table.insert(cmake.defines, { 'CURL_USE_LIBRTMP', false })
      table.insert(cmake.defines, { 'USE_LIBIDN2', false })
      table.insert(cmake.defines, { 'USE_NGHTTP2', false })
      table.insert(cmake.defines, { 'CURL_DISABLE_LDAP', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_LDAPS', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_RTSP', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_FTP', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_FILE', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_TELNET', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_DICT', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_GOPHER', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_IMAP', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_POP3', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_SMTP', true })
      table.insert(cmake.defines, { 'CURL_DISABLE_TFTP', true })
    end

    builder:cmake(cmake)
  end,
})

return recipe
