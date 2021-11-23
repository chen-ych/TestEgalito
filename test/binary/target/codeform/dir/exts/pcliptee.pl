use Win32::Clipboard;

my $CLIP = Win32::Clipboard();

my @data = <>;
my $text = join('', @data);

$CLIP->Set($text);

print $text;
