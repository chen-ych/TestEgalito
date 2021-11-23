use Win32::Clipboard;

my $CLIP = Win32::Clipboard();

my @data = <>;

$CLIP->Set(join('', @data));
