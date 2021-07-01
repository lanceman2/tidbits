
# This file is part of the quickbuild package build system

class QBPackage < Hash

    attr_reader :path               # path of file qb.package_info
    attr_reader :gen_sub_prog_file
    attr_reader :name

    private

    def init_path(path, name, prefix, dir = '')
        unless path[name]
            path[name] = [ ]
            path[name][1] = name.to_s +
                ".  The default value is " +
                prefix.to_s.upcase + dir
            if prefix.instance_of? String
                path[name][0] = File.expand_path prefix + dir
            else
                path[name][0] = File.expand_path path[prefix][0] + dir
            end
        end
    end

    def reset_path(path, name, prefix, dir = '')
        # Reset the path in case the prefix changed when we parsed the
        # argument line options, unless the user set this particular path
        # in an argument line options.
        unless path[name][2]
            path[name][1] = name.to_s
            if prefix.instance_of? String
                path[name][0] = File.expand_path prefix + dir
            else
                path[name][0] = File.expand_path path[prefix][0] + dir
            end
        end
    end


    def set_standard_paths

        if self[:info][:STANDARD_PATHS]

            if @stage == :init
                set = self.method(:init_path)
            else # @stage == :user
                set = self.method(:reset_path)
            end
        
            path = self[:option][:path]

            # See GNU autotool configure -h for list with descriptions of
            # these paths
            #
            # The dependent paths must be set after the prerequisite prefix
            # paths.

            set.call(path, :prefix,         'installed')
            set.call(path, :eprefix,        :prefix)
            set.call(path, :bindir,         :eprefix, '/bin')
            set.call(path, :sbindir,        :eprefix, '/sbin')
            set.call(path, :libexecdir,     :eprefix, '/libexec')
            set.call(path, :sysconfdir,     :prefix , '/etc')
            set.call(path, :sharedstatedir, :prefix , '/com')
            set.call(path, :localstatedir,  :prefix , '/var')
            set.call(path, :libdir,         :eprefix, '/lib')
            set.call(path, :pkgconfigdir,   :libdir,  '/pkgconfig')
            set.call(path, :includedir,     :prefix , '/include')
            set.call(path, :oldincludedir,  '/usr/include')
            set.call(path, :datarootdir,    :prefix , '/share')
            set.call(path, :datadir,        :datarootdir)
            set.call(path, :infodir,        :datarootdir, '/info')
            set.call(path, :localedir,      :datarootdir, '/locale')
            set.call(path, :mandir,         :datarootdir, '/man')
            set.call(path, :docdir,         :datarootdir, '/doc/' +
                                            self[:info][:NAME])
            set.call(path, :htmldir,        :docdir)
            set.call(path, :dvidir,         :docdir)
            set.call(path, :pdfdir,         :docdir)
            set.call(path, :psdir,          :docdir)
        end
    end

    def help_opt(f, pre, text)

        max = 76 # length of printed text line
        spaces = "                         " # indent
        indent = spaces.length
        charPerLine = max - indent
        itext = 0 # test index printed so far
        textLen = text.length

        pre = "  " + pre

        f.print pre

        if pre.length < indent
            # pad to indent position
            f.print spaces[0, indent - pre.length]
            f.print text[0, charPerLine]
            itext += charPerLine
        end

        f.print "\n"

        while itext < textLen do
            f.printf spaces + text[itext, charPerLine] + "\n"
            itext += charPerLine
        end

        f.print "\n"
    end


    def set( info, aname, name)
        info[aname] = info[name] unless info[aname]
    end


    public

    def parse_user_config_opts (opt_args)

        $stdin.print 'Warning: ' + caller(0).first.split.last +
            " has been called already\n" unless @stage == :user

        arg = nil

        if opt_args
            args = opt_args.dup
            ret = ''
        end
        arg = args.shift if args

        while arg do

            got = false
            val = nil
            opt = nil

            # get opt and val from --opt=val or --opt val
            if arg =~ /^--[^=]+=[^=]+$/
                opt = arg.sub(/^--/,'').sub(/=[^=]*$/, '').gsub('-', '_')
                val = arg.sub(/^--[^=]+=/, '')
            elsif arg =~ /^--[^=]+$/
                opt = arg.sub(/^--/,'').gsub('-', '_')
                if args[0] and (args[0] =~ /^[^-=]+[^=]*$/ or args[0] == '-')
                    val = arg = args.shift
                end
            else
                ret += 'bad option: ' + arg + "\n"
                arg = args.shift
                next
            end

            arg = args.shift

            self[:option][:bool].each do |k,v|
                if opt == k.to_s
                    got = true
                    v[2] = true # the user set it
                    if not val
                        v[0] = true
                    elsif  val =~ /(^n|^f|^N|^F|^0|^of|^Of|^oF|^OF)/
                        v[0] = false
                    else
                        v[0] = true
                    end
                    break
                end
            end
            next if got

            if val
                [ self[:option][:path], self[:option][:string] ].each do |h|
                    h.each do |k,v|
                        if opt == k.to_s
                            got = true
                            v[2] = true # the user set it
                            if h == self[:option][:path]
                                v[0] = File.expand_path val
                            else
                                v[0] = val
                            end
                            break
                        end
                    end
                    break if got
                end
                next if got
            end

            ret += 'bad option: --' + opt
            ret += ' ' + val.to_s if val
            ret += "\n"
        end


        set_standard_paths

        @stage = :final


        if ret.length > 0
            $stderr.print "Warning: " + ret + "\n"
            throw ret
        end

    end

    def sub_strings (infile, outfile, header = nil)

        if infile == '-'
            fin = $stdin
        else
            fin = File.open(infile, 'r')
        end

        if outfile.kind_of? File
            out = outfile
        elsif outfile == '-'
            out = $stdout
        else
            out = File.open(outfile, 'w')
        end


        fin.each_line do |line|
            [self[:info], self[:option][:path], self[:option][:bool],
            self[:option][:string] ].each do |h|
                h.each do |k,v|
                    if v.instance_of? String
                        val = v
                    elsif v.instance_of? Array
                        if v[0].instance_of? String
                            val = v[0]
                        else
                            val = ((v[0])?'1':'0')
                        end
                    else
                        val = ((v)?'1':'0')
                    end
                    line.gsub!('@' + k.to_s + '@', val)
                end
            end
        
            out.write line
        end

        fin.close unless infile == '-'

        out.write header if header

        out.close unless (outfile == '-' or outfile == out)

    end


    @@rerun_command_cache = nil


    def get_rerun_command

        return @@rerun_command_cache if @@rerun_command_cache

        @@rerun_command_cache = __FILE__

        self[:option].each do |hk,hv|
            hv.each do |k,v|
                if v[2] # user set it
                    @@rerun_command_cache += ' --' +
                        k.to_s + '=' + v[0].to_s
                end
            end
        end

        @@rerun_command_cache
    end


    @@self = nil


    def initialize (package_info_file = nil, user_config_opts = nil,
                    info_defaults = nil)

        throw "You cannot make more than one QBPackage" if @@self
        @@self = self

        

        # a set of strings that describe the package and
        # how it is built in the default case the file
        # loaded in path may change any or theses.
        self[:info] = {
            # setup required info with defaults
                :VERSION => '0.0.0',
                :QUICKBUILD_VERSION => $qb_version,
                :NAME => 'package',
                :URL => 'not set',
                :DESCRIPTION => 'generic C software package',
                :CONFIG_H => 'qb_config.h',
                :STANDARD_PATHS => true,
                :PACKAGE_ALIASES => true
        }
        # option changes when package builder sets up the build
        # while the info stays constant
        self[:option] = { :bool => { }, :string => { }, :path => { } }

        if info_defaults
            self[:info].each do |k,v|
                # changing as I iterate through it.  Is that a problem?
                self[:info][k] = info_defaults[k] if info_defaults[k]
            end
        end


        if package_info_file

            @path = File.expand_path package_info_file

            # load the package developers fixed package information
            # that is fixed for a given release or tarball release
            load @path

            if defined? set_package_info
                set_package_info( self[:info], self[:option] )
            else
                throw "method set_package_info was not found in " + @path
            end

        end

        info = self[:info]

        self[:info][:NAME].gsub!(/ /,'_')

        @name = self[:info][:NAME]

        if info[:PACKAGE_ALIASES]
            set info, :TARNAME,      :NAME
            set info, :PACKAGE_NAME, :NAME
            set info, :PACKAGE,      :NAME
            set info, :DESCRIPTION,  :NAME

            set info, :PACKAGE_URL,  :URL
            set info, :HOMEPAGE,     :URL
            set info, :BUG_REPORT,   :URL
        end

        info.freeze

        @stage = :init

        set_standard_paths

        @stage = :user

        self[:option].each do |type, opt|
            opt.each do |k,v|
                # freeze the descriptions
                v[1].freeze
            end
        end

        self[:option][:path].each do |k,v|
            v[0] = File.expand_path v[0]
        end

        parse_user_config_opts user_config_opts if user_config_opts

    end # initialize


    def get_user_usage_config_opts (file)

        [self[:option][:string], self[:option][:bool],
            self[:option][:path]].each do |h|
            h.each do |k,v|
                if v[0].instance_of? String
                    arg = k.to_s.upcase
                else # Bool
                    arg = '[yes|no]'
                end
                help_opt(file,
                    " --#{k.to_s} #{arg}",
                    v[1].to_s +
                    "  The current default value is " +
                    v[0].to_s + "\n")
            end
        end
    end


    def gen_config_h (path = '-')

        if path == '-'
            f = $stdout
        else
            f = File.open(path, 'w')
        end

        f.write <<-END
/* This is a generated file */

        END

        self[:info].each do |k,v|
            if v.instance_of? String
                val = ' "' + v + "\"\n\n"
            else
                val = if v then " (1)\n\n" else " (0)\n\n" end
            end
            f.print '#define ' + k.to_s + val
        end


        [ self[:option][:path], self[:option][:string] ].each do |h|
            h.each do |k,v|
                f.print '/* ' + v[1] + " */\n"
                f.print '#define ' + k.to_s.upcase +
                    ' "' + (v[0]).to_s + "\"\n\n"
            end
        end

        self[:option][:bool].each do |k,v|
            f.print '/* ' + v[1] + " */\n"
            f.print '#define ' + k.to_s.upcase +
                ' (' + ((v[0])?'1':'0') + ")\n\n"
        end


        f.close unless path == '-'

    end

    def gen_sub_prog ( path = '-' )

        if path == '-'
            f = $stdout
        else
            f = File.open(path, 'w')
        end

        @gen_sub_prog_file = path
        
        f.print <<-END
#!/usr/bin/ruby -w
######################################
###### This is a generated file ######
######################################


def usage

puts <<EEND

  Usage: \#{$0} INFILE OUTFILE [HEADER ...]

  Subsitute @strings@ in INFILE and create OUTFILE.
  If INFILE is '-' use stdin.  If OUTFILE is '-' use
  stdout.  Newlines are added after each HEADER string.

EEND
exit 1

end # usage()

if ARGV.length < 2
    usage
end


if ARGV[0] == '-'
  fin = $stdin
else
  fin = File.open(ARGV[0], 'r')
end

if ARGV[1] == '-'
  out = $stdout
  outpath = nil
else
  out = File.open(ARGV[1], 'w')
  outpath = ARGV[1]
end

begin

  ARGV.shift(2)

  ARGV.each do |arg|
    out.write arg + "\\n"
  end

  pkg = #{self.to_s}

  fin.each_line do |line|
    [pkg[:info], pkg[:option][:path], pkg[:option][:bool],
    pkg[:option][:string] ].each do |h|
        h.each do |k,v|
            if v.instance_of? String
                val = v
            elsif v.instance_of? Array
                if v[0].instance_of? String
                    val = v[0]
                else
                    val = ((v[0])?'1':'0')
                end
            else
                val = ((v)?'1':'0')
            end
            line.gsub!('@' + k.to_s + '@', val)
        end
    end
    out.write line
  end

rescue

  if outpath
    # It's all or nothing.
    out.close
    # remove the file that we just made.
    FileUtils.unlink outpath
    exit 1 # error return code
  end

  #success

end

        END

    File.chmod(0755, path) unless path == '-'
    
    end


    def gen_make (file)

        self[:info].each do |k,v|
            if v.instance_of? String
                file.write k.to_s + ' = ' + v + "\n"
            elsif v
                file.write k.to_s + " = true\n"
            else
                file.write "# #{k.to_s} is not set\n"
            end
        end

        [self[:option][:string], self[:option][:bool],
            self[:option][:path]].each do |h|
            h.each do |k,v|
                if v[0].instance_of? String
                    file.write k.to_s + ' = ' + v[0] + "\n"
                elsif v[0]
                    file.write k.to_s + " = true\n"
                else
                    file.write "# #{k.to_s} is not set\n"
                end
            end
        end

    end

end


