#!/bin/python3

from pathlib import Path
from argparse import ArgumentParser, RawDescriptionHelpFormatter
import shutil
import sys
import os
import subprocess
from typing import List

LHF_DOCKER_IMAGE: Path = Path('lhf-artifact-docker.tar.gz')
LHF_DOCKER_BIND_PATH: Path = Path('workdir')
LHF_DOCKER_CONTAINER_BIND_PATH: Path = Path('/workdir')
LHF_DOCKER_WORKDIR: Path = Path('/workdir')
LHF_DOCKER_REGISTERED_IMAGE_NAME: str = "lhf-artifact-docker:latest"
LHF_DOCKER_CONTAINER_NAME: str = "LHF_TOOL_TEST_container"
DOCKER_COMMAND: str = 'docker'


command_help: str = f'''
Please ensure that you have docker installed and have the appropriate
permissions to use it.

Various commands are explained here:
    shell            Start a container and open into an empty root bash shell.

    attach           Attach to an existing, running container in the background.

    kill             Stop any running container of LHF.

    clean            Stop and remove any running containers and remove images.

This script assumes the following constants:
    Testing environment image archive path: '{LHF_DOCKER_IMAGE}'
    Name used when image is loaded into docker: '{LHF_DOCKER_REGISTERED_IMAGE_NAME}'
    Launched container name: '{LHF_DOCKER_CONTAINER_NAME}'
    Docker command: '{DOCKER_COMMAND}'
'''

class Config:

	def __init__(self) -> None:
		self.command: str = 'invalid'
		self.background: bool = False
		self.no_debug: bool = False
		self.tmux: bool = False
		self.gdb: bool = False
		self.valgrind: bool = False
		self.vflags: List[str] = []
		self.lflags: List[str] = []

def init_argparser() -> Config:
	parser = ArgumentParser(
		formatter_class=RawDescriptionHelpFormatter,
		description=f"Run various artifact tests using the docker image.",
		epilog=command_help
	)

	parser.add_argument(
		'command',
		help="Specify the command to execute.",
		choices=(
			'shell',
			'attach',
			'kill',
			'clean'))

	parser.add_argument(
		'--tmux', '-t',
		action='store_true',
		help="Start the container with a tmux session instead of just bash.")

	parser.add_argument(
		'--background', '-b',
		action='store_true',
		help="Start the docker container in the background.")

	# parser.add_argument(
	# 	'--no-debug', '-nd',
	# 	action='store_true',
	# 	help="Disable debugging symbols.")

	# parser.add_argument(
	# 	'--gdb', '-g',
	# 	action='store_true',
	# 	help="Enable GDB for this run.")

	# parser.add_argument(
	# 	'--valgrind', '-m',
	# 	help="enable Valgrind for this run.")

	# parser.add_argument(
	# 	'--vflag', '-vf',
	# 	action='append',
	# 	default=[],
	# 	help="Add a flag to a Valgrind invocation. Implicitly enables Valgrind.")

	# parser.add_argument(
	# 	'--lflag', '-lf',
	# 	action='append',
	# 	default=[],
	# 	help="Specify a CMake flag for LHF compilation.")

	result = parser.parse_args()

	config = Config()

	config.command    = result.command
	config.tmux       = result.tmux
	config.background = result.background
	# config.no_debug   = result.no_debug
	# config.gdb        = result.gdb
	# config.valgrind   = result.valgrind
	# config.vflags     = result.vflag
	# config.lflags     = result.lflag

	return config

def run_docker_command(interactive: bool, command: List[str], nocapture: bool = False) -> str:
	if not shutil.which(DOCKER_COMMAND):
		print(f"Error: `{DOCKER_COMMAND}` does not seem to be installed. Please "
			   "install docker")
	print(f"RUNNING: `{DOCKER_COMMAND} {' '.join(command)}`")
	if interactive:
		result = os.execlp(DOCKER_COMMAND, DOCKER_COMMAND, *command)

	elif not nocapture:
		result = subprocess.run(
			[DOCKER_COMMAND, *command],
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT
		)

		if result.returncode != 0:
			print(f"Could not run `{DOCKER_COMMAND} {' '.join(command)}`: '{result.stdout.decode()}'")
			sys.exit(1)

		return result.stdout.decode()

	else:
		result = subprocess.run(
			[DOCKER_COMMAND, *command],
			stderr=subprocess.STDOUT
		)

		if result.returncode != 0:
			print(f"Command failed. Exiting.'")
			sys.exit(1)

		return ''

def check_image_registered() -> bool:
	result = run_docker_command(False, ['images', '-q', str(LHF_DOCKER_REGISTERED_IMAGE_NAME)])
	return len(result.strip()) > 0

def get_containers_running() -> List[str]:
	result = run_docker_command(False, ['ps', '-q', '-f', f'name={LHF_DOCKER_CONTAINER_NAME}'])
	return result.strip().split()

def get_containers_present() -> List[str]:
	result = run_docker_command(False, ['ps', '-a', '-q', '-f', f'name={LHF_DOCKER_CONTAINER_NAME}'])
	return result.strip().split()

def load_image():
	if not LHF_DOCKER_IMAGE.exists():
		print(f"Error: {LHF_DOCKER_IMAGE} does not exist.")
		sys.exit(1)
	run_docker_command(False, ['load', '--input', str(LHF_DOCKER_IMAGE)])

def remove_image():
	run_docker_command(False, ['rmi', str(LHF_DOCKER_REGISTERED_IMAGE_NAME)])

def start_container(command_list: List[str] = [], background: bool = False):
	if not check_image_registered():
		print(f"Error: {LHF_DOCKER_REGISTERED_IMAGE_NAME} is not registered.")
		sys.exit(1)
	if background:
		run_docker_command(False,
			['run',
				'--name', LHF_DOCKER_CONTAINER_NAME,
				'-dit',
				'-v', f'{LHF_DOCKER_BIND_PATH.resolve()}:{LHF_DOCKER_CONTAINER_BIND_PATH}',
				str(LHF_DOCKER_REGISTERED_IMAGE_NAME),
				*command_list])
	else:
		run_docker_command(True,
			['run',
				'--name', LHF_DOCKER_CONTAINER_NAME,
				'-v', f'{LHF_DOCKER_BIND_PATH.resolve()}:{LHF_DOCKER_CONTAINER_BIND_PATH}',
				'-it', str(LHF_DOCKER_REGISTERED_IMAGE_NAME),
				*command_list])

def exec_container(
	command_list: List[str],
	cwd_path: str | None = None,
	interactive: bool = False,
	nocapture: bool = False,
	user_mode: bool = False):
	cwdparam = []
	interactiveparam = []

	if cwd_path != None:
		cwdparam = ['-w', str(LHF_DOCKER_WORKDIR / cwd_path)]

	if interactive:
		interactiveparam = ['-it']

	if user_mode:
		run_docker_command(interactive,
			['exec',
				*interactiveparam,
				'-u', f'{os.getpid()}:{os.getgid()}',
				*cwdparam,
				LHF_DOCKER_CONTAINER_NAME,
				*command_list], nocapture)
	else:
		run_docker_command(interactive,
			['exec',
				*interactiveparam,
				*cwdparam,
				LHF_DOCKER_CONTAINER_NAME,
				*command_list], nocapture)

def exec_step(
	command_list: List[str],
	cwd_path: str | None = None):
	exec_container(command_list, cwd_path, interactive = False, nocapture = True)

def exec_handover(
	command_list: List[str],
	cwd_path: str | None = None):
	exec_container(command_list, cwd_path, interactive = True)

def kill_container():
	run_docker_command(False, ['kill', LHF_DOCKER_CONTAINER_NAME])

def remove_stopped_containers():
	run_docker_command(False, ['rm', *get_containers_present()])

def main():
	config: Config = init_argparser()

	if config.command == 'shell':
		if not check_image_registered():
			print("Image not registered. Registering image...")
			load_image()

		if config.tmux:
			start_container(["tmux"], background=config.background)
		else:
			start_container(["bash"], background=config.background)

	elif config.command == 'exec':
		if not check_image_registered():
			print("Image not registered. Registering image...")
			load_image()

		if config.tmux:
			exec_handover(["tmux", "attach"])
		else:
			exec_handover(["bash"])

	elif config.command == 'attach':
		if not check_image_registered():
			print("Image not registered. Registering image...")
			load_image()

		if config.tmux:
			exec_handover(["tmux", "attach"])
		else:
			exec_handover(["bash"])

	elif config.command == 'kill':
		if len(get_containers_running()) > 0:
			kill_container()
		else:
			print("Error: no running containers found.")

	elif config.command == 'clean':
		if len(get_containers_running()) > 0:
			kill_container()

		if len(get_containers_present()) > 0:
			remove_stopped_containers()

		if check_image_registered():
			remove_image()
	else:
		print(f"Error: Invalid command: '{config.command}'")
		print("Exiting.")
		sys.exit(1)

if __name__ == '__main__':
	main()