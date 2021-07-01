# Open OnDemand Portal

# We started with ood-portal-generator but had to edit the result
# Started using ood-portal-generator version 0.8.0
#
Listen 9090

DocumentRoot public


<VirtualHost *:9090>


  # BUG 1 in Apache httpd version 2.4.41, and likely many versions before
  # and after: SetEnv does not get env values into lua scripts but
  # SetEnvIf does.  SetEnvIf will be slower, but at least it will work.
  # It'd be nice if we could use: SetEnvIf true bla=bla

  # BUG 2 in Apache SetEnvIf does not handle:
  # SetEnvIf Request_Method . FOO="3"
  # it makes FOO = "3"  and not  FOO = 3
  # SetEnvIf Request_Method . OOD_PUN_STAGE_CMD="sudo foo"
  # makes: OOD_PUN_STAGE_CMD = "sudo
  # and so we do not know how to have a space in the string.

  #
  # In SetEnvIf Request_Method .
  # the Request_Method . will always be true, so it's a slower version of
  # SetEnv.

  SetEnvIf Request_Method . NGINX_STAGE_CONFIG_FILE=@ROOTDIR@/conf/nginx_stage.yml
  SetEnvIf Request_Method . OOD_NGINX_URI=/nginx
  SetEnvIf Request_Method . OOD_PUN_URI=/pun

  # Authenticated-user to system-user mapping configuration
  #
  SetEnvIf Request_Method . OOD_USER_MAP_CMD=@ONDEMAND_PREFIX@/ood_auth_map/bin/ood_auth_map.regex
  SetEnvIf Request_Method . OOD_USER_ENV=REMOTE_USER

  # Per-user Nginx (PUN) configuration
  # NB: Apache will need sudo privs to control the PUNs
  #
  #SetEnvIf Request_Method . OOD_PUN_STAGE_CMD="sudo @ONDEMAND_PREFIX@/nginx_stage/sbin/nginx_stage"
  # This path is seen from the script file nginx_stage.lua so we need the
  # full path.
  SetEnvIf Request_Method . OOD_PUN_STAGE_CMD=@ROOTDIR@/pun_nginx_stage


  # Lua configuration
  #
  LuaRoot "@ONDEMAND_PREFIX@/mod_ood_proxy/lib"
  LogLevel lua_module:debug

  # Log authenticated user requests (requires min log level: info)
  LuaHookLog logger.lua logger

  #
  # Below is used for sub-uri's this Open OnDemand portal supports
  #

  # Serve up publicly available assets from local file system:
  #
  #     http://localhost:9090/public/favicon.ico
  #     #=> public/favicon.ico
  #
  Alias "/public" "public"
  <Directory "public">
    Options Indexes FollowSymLinks
    AllowOverride None
    Require all granted
  </Directory>


  # Reverse proxy traffic to backend PUNs through Unix domain sockets:
  #
  #     http://localhost:9090/pun/dev/app/simulations/1
  #     #=> unix:/path/to/socket|http://localhost/pun/dev/app/simulations/1
  #
  <Location "/pun">
    
    #Require all granted

    AuthType Basic
    AuthName "private"
    AuthUserFile "conf/htpasswd"
    RequestHeader unset Authorization
    Require valid-user

    ProxyPassReverse "http://localhost:9090/pun"

    # ProxyPassReverseCookieDomain implementation (strip domain)
    Header edit* Set-Cookie ";\s*(?i)Domain[^;]*" ""

    # ProxyPassReverseCookiePath implementation (less restrictive)
    Header edit* Set-Cookie ";\s*(?i)Path\s*=(?-i)(?!\s*/pun)[^;]*" "; Path=/pun"

    SetEnvIf Request_Method . OOD_PUN_SOCKET_ROOT=pun
    SetEnvIf Request_Method . OOD_PUN_MAX_RETRIES=3

    LuaHookFixups pun_proxy.lua pun_proxy_handler

  </Location>

  # Control backend PUN for authenticated user:
  # NB: See mod_ood_proxy for more details.
  #
  #    http://localhost:9090/nginx/stop
  #    #=> stops the authenticated user's PUN
  #
  <Location "/nginx">
    AuthType Basic
    AuthName "private"
    AuthUserFile "conf/htpasswd"
    RequestHeader unset Authorization
    Require valid-user

    LuaHookFixups nginx.lua nginx_handler
  </Location>

  # Redirect root URI to specified URI
  #
  #     http://localhost:9090/
  #     #=> http://localhost:9090/apps/dashboard
  #
  RedirectMatch ^/$ "/apps/dashboard"

  # Redirect logout URI to specified redirect URI
  #
  #     http://localhost:9090/logout
  #     #=> http://localhost:9090/apps/dashboard/logout
  #
  Redirect "/logout" "/apps/dashboard/logout"



</VirtualHost>
