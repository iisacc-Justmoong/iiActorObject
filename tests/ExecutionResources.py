"""Temporary resource links for explicitly requested application checks."""
from contextlib import contextmanager
import os
from pathlib import Path
from tempfile import TemporaryDirectory

PROJECT = Path(__file__).resolve().parent.parent
RESOURCES = (PROJECT/'resources').resolve(strict=True)

class ExecutionResources:
    def __init__(self, resource_directory, output_directory):
        self.resource_directory = Path(resource_directory)
        self.output_directory = Path(output_directory)

    def link(self, source, name):
        source = Path(source).resolve(strict=True)
        if RESOURCES not in source.parents or not source.is_file():
            raise ValueError('Execution inputs must be files under resources/')
        if not name or Path(name).name != name or name in ('.', '..'):
            raise ValueError('The link name must be a single file name')
        destination = self.output_directory/name
        os.link(source, destination)
        return destination

@contextmanager
def execution_workspace(output_root):
    build = (PROJECT/'build').resolve(strict=True)
    output_root = Path(output_root).resolve(strict=False)
    if output_root != build and build not in output_root.parents:
        raise ValueError('Execution output must be inside build/')
    output_root.mkdir(parents=True, exist_ok=True)
    output_root = output_root.resolve(strict=True)
    if output_root != build and build not in output_root.parents:
        raise ValueError('Execution output must be inside build/')
    with TemporaryDirectory(prefix='.execution-resource-', dir=RESOURCES) as resource_directory:
        with TemporaryDirectory(prefix='execution-', dir=output_root) as output_directory:
            yield ExecutionResources(resource_directory, output_directory)
