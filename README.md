# AppDynamics

AppDynamics integration for Rails and other Rack apps.

## Requirements

For reporting, the AppDynamics Ruby agent requires Linux. On other platforms, the gem
will function in a no-reporting mode.

## Install Gem

```rb
# Gemfile

gem 'appdynamics'
```

## Upgrading

## Rails Setup

### Collector Configuration

At the minimum, you must configure the application. You can either do this via a config file or via ENV vars.

```yml
# config/appdynamics.yml
app_name: My App
tier_name: Rails
node_name: Production
controller:
  host: mycontroller.com
  port: 443 # Optional
  account: myaccount
  access_key: accesskey
  use_ssl: true # Optional
```
_Alternatively:_

```
APPD_APP_NAME="My App"
APPD_TIER_NAME=Rails
APPD_NODE_NAME=Production
APPD_CONTROLLER_HOST=443
APPD_CONTROLLER_ACCOUNT=myaccount
APPD_CONTROLLER_ACCESS_KEY=accesskey
APPD_CONTROLLER_USE_SSL=true
```

#### Node Names for Multi-Process Servers

Each application process running on a server must have a unique App, Tier, Node tuple. Many Ruby servers (e.g. Puma, Unicorn) will start multiple processes for each application. The  AppDynamics Ruby agent will handle this by automatically appending an index to additional processes.

To track the index of the process, a series of "nodeindex" files are used. By default these are located at `APP_ROOT/tmp/appdynamics/nodeindex`. In the event that this path in not writable you can change it by setting `nodeindex_path` in your config (or `APPD_NODEINDEX_PATH` in your environment).

### Defining Business Transactions

By default, the agent will use the first two segments of your URL as the name of your Business Transactions. While this will
get you started, you'll likely find that it isn't an ideal setup for most RESTful Rails applications with URLs like `/users/:id`.
to customize the behavior you can set up an initializer:

```rb
# config/initializers/appdynamics.rb

AppDynamics::BusinessTransactions.define do
  # Your BT Config here.
end
```

You can both change how the auto detected BTs are named (or completely disable it), as well as manually specify naming for specific URL
and HTTP method combinations. You can use both auto detection with manual naming. If a manual name matches, it will take precedence.
If auto detection is disabled, unmatched BTs will not be logged.

#### Auto Configuration

```rb
# Disable auto BTs
auto false

# Change number of segments
auto segments: { first: 3 } # First 3 segments
auto segments: { last: 1 } # Last 1 segment

# Use full URL
auto full: true

# Dynamic BT names
auto dynamic: { param: :search } # /one/two/three?search=query => /one/two.query
auto segments: { first: 1 }, dynamics: { param: :search} # /one/two/three?search=query => /one.query
auto dynamic: { header: "Content-Type" } # CONTENT_TYPE=application/json /one/two/three.json => /one/two.application/json
auto dynamic: :method # POST /one/two/three => /one/two.POST
auto dynamic: :host # http://example.com/one/two/three => /one/two.example.com
auto dynamic: :origin # HTTP_ORIGIN=http://other.com http://example.com/one/two/three => /one/two.other.com
```

#### Manual Configuration

A manual Business Transaction is specified as a name mapping to a path matcher which can
be a String (for an exact match) or a Regex or an array of either type.

```rb
bt "users" => "/users"
bt "admin" => %r{^/admin/?}
bt "companies" => ["/company_list", "/companies", %r{/company/\d+}]
```

You can also specify an HTTP method or list of methods to limit to only requests
of that type. Available options are `:get`, `:post`, `:put`, `:patch`, and `:delete`.

```rb
bt "new_user" => "/users", method: :post
bt "update_user" => "/users", method: [:put, :patch]
```
Finally, more complex definitions can be made using the block API. Inside of the blockja list of path matchers is specified for a specific HTTP method.

```rb
bt "api" do
  get "/users"
  post "/users", %r{.+\.json$}
end
```

## Miscellany

**Enable in Rails Development Mode**

Normally, we don’t recommend instrumenting your development environment. However, for sake of testing, this might be useful. To do so, update your `config/application.rb` to include:

`config.appdynamics.environments << :development`

**View Initial Documentation**

Download the code and run `bundle exec yard` in it. You can then open `doc/index.html` in a browser.

**Alternate C/C++ SDK**

The Ruby agent uses the AppDynamics C/C++ SDK, which is bundled with the gem. An alternate version (API compatible with 4.4/4.5) can be used by passing `--with-appdynamics-dir` to the installation. With bundler, you can set this as a config option:

```sh
bundle config build.appdynamics --with-appdynamics-dir={INSTALL_PATH}
```
