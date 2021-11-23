#!/usr/bin/perl
# codeform.pl by DWK
# Perl CGI script to execute codeform on a remote server

# Located at:
# http://dwks.theprogrammingsite.com/myprogs/down/codeform_online/codeform.pl

# Accessed from:
# http://dwks.theprogrammingsite.com/myprogs/down/codeform_online/cfonline.htm

use CGI qw/:standard/;

my $max = 32768;  # Maximum length in bytes of code (truncuated to this size)

# Valid compound rules files (excluding directory) for codeform
my @formats = qw/
    c_1_html
    c_1_css
    c_1_html
    c_1_vbb
    cpp_1_css
    cpp_1_html
    cpp_1_vbb
    java_1_css
    java_1_htm
    java_1_vbb
/;

# Valid input file formats for codeform
my @inputs = qw/
    c
    cpp
    java
/;

# Valid output file formats for codeform
my @outputs = qw/
    html
    vbb
    css
/;

# Valid styles for codeform
my @styles = qw/
    1
    devcpp
    kate
    scite
/;

# Valid extra rules files for codeform
my @extras = qw/
    winapi
/;

&main();

sub main {
    print header;
    print "<html><head><title>Codeform version 1.2.0 online -- finished</title>\n"
        . "</head>\n"
        . "<body><h1>Codeform version 1.2.0 online -- finished</h1>\n";
    
    &execute_codeform(&generate_rules_file_list() . &extra_parameters());
    
    print "<input type=\"button\" onClick=\"history.go(-1)\" value=\"Back\" />\n";
    print "</body></html>\n";
}

sub parse_optional_parameter {
    my $filename = param($_[0]);
    
    if(!defined($filename)) {
        return undef;
    }
    
    $filename =~ s/[^\w_\d]//;
    return $filename;
}

sub parse_parameter {
    my $filename = parse_optional_parameter(@_);
    
    if(!defined($filename)) {
        print "$0: Undefined parameter: $param\n";
        exit(1);
    }
    
    return $filename;
}

sub find_in_array {
    my $look = shift;
    
    foreach(@_) {
        if($look eq $_) {
            return 1;  # $look exists in the list
        }
    }
    
    return 0;
}

sub validate_name {
    if(!&find_in_array(@_)) {
        print "$0: Unknown format: \"$_[0]\"\n";
        exit(1);
    }
}

sub extra_parameters {
    my $p = '';
    my $extra = '';

    foreach(@extras) {
        $p = parse_optional_parameter($_);
        if(defined($p)) {
            $extra .= ' -f ./rules/' . $_;
        }
    }
    
    return $extra;
}

sub generate_rules_file_list {
    # Get the data from the parameters
    my $input = &parse_parameter('input');
    my $output = &parse_parameter('output');
    my $style = &parse_parameter('style');
    
    # Try a compound rule
    my $format = "./rules/" . $input . "_" . $style . "_" . $output;
    
    if(!&find_in_array($format, @formats)) {  # The format is not a compound
        $format = &non_compound_format($input, $output, $style);
    }
    
    print "<pre>Input format: $input\n"
        . "Output format: $output\n"
        . "Style: $style</pre>\n";
    
    return $format;
}

sub non_compound_format {
    my $input = $_[0];
    my $output = $_[1];
    my $style = $_[2];
    my $format = '';
    
    &validate_name($input, @inputs);
    &validate_name($output, @outputs);
    &validate_name($style, @styles);
    
    if($input eq 'cpp') {  # Use ordinary C rules and add ./rules/cpp for C++
        $format = "./rules/_" . $output . " -f ./rules/c_" . $style
            . " -f ./rules/cpp";
    }
    else {
        $format = "./rules/_" . $output . " -f ./rules/" . $input . "_"
            . $style;
    }
    
    return $format;
}

sub execute_codeform {
    my $command = "./codeform -f $_[0] -f rules/online";
    
    print "<pre>`$command`</pre>\n";
    
    print "<p><b><big>Code:</big></b><br />\n"
        . "<textarea name=\"code\" rows=\"25\" cols=\"80\">\n";
    
    if(!open(CODEFORM, "|$command")) {
        print "$0: Can't open pipe: \"$command\"\n";
        exit(1);
    }
    
    $code = substr(param('code'), 0, $max);  # Limit the code to $max characters
    
    print CODEFORM $code;
    close CODEFORM;
    
    print "</textarea></p>\n";
}