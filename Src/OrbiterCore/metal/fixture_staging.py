"""Stage native fixtures in a fresh owned tree without writing through source links."""
import os
from pathlib import Path
import tempfile


def stage(content, output, prefix='content-'):
    content = Path(content).resolve()
    launch = Path(tempfile.mkdtemp(prefix=prefix, dir=output))
    # Reserve real directories AND fixture files before introducing any links.
    # lexists asks the destination filesystem about aliases (case/normalization),
    # including dangling links; string spelling comparisons are not sufficient.
    fixtures = Path(__file__).resolve().with_name('fixtures')
    for folder, filename in [('Config', 'MetalCamera.cfg'), ('Scenarios', 'MetalCamera.scn')]:
        directory = launch / folder
        directory.mkdir()
        with (directory / filename).open('xb') as target:
            target.write((fixtures / filename).read_bytes())
    for source in content.iterdir():
        destination = launch / source.name
        if os.path.lexists(destination):
            continue
        if source.is_dir() or source.suffix.lower() == '.dylib':
            destination.symlink_to(source, target_is_directory=source.is_dir())
        elif source.suffix.lower() == '.cfg':
            with destination.open('xb') as target:
                target.write(source.read_bytes())
    for source in (content / 'Config').iterdir():
        destination = launch / 'Config' / source.name
        if not os.path.lexists(destination):
            destination.symlink_to(source, target_is_directory=source.is_dir())
    # Preserve the fresh tree for evidence on success or failure. Never remove
    # an unchecked caller-supplied output path or reuse an older launch root.
    return launch
