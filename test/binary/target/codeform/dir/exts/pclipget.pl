use Win32::Clipboard;

my $CLIP = Win32::Clipboard();

print $CLIP->GetText();
