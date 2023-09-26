import pytest
from typing import NamedTuple
import random
import string
import requests
from .api_types import CreateAccountRequest

_port = 8080
_api_base = f'http://localhost:{_port}/api'

# General utilities
def api_endpoint(ep: str)->str:
    ''' Returns the full URL of an API endpoint. '''
    return f'{_api_base}/{ep}'


def ws_endpoint()->str:
    ''' Returns the full URL of the websocket endpoint. '''
    return f'ws://localhost:{_port}/api/ws'


def check_status(res: requests.Response) -> requests.Response:
    '''
    Checks the status of a response. Unlike Response.raise_for_status,
    this includes the response in the exception.
    '''
    if not res.ok:
        raise RuntimeError(
            f'Request to {res.url} returned {res.status_code}\nRequest: {res.request.body}\nResponse: {res.text}')
    return res


def _gen_identifier() -> str:
    '''
    Generates a random identifier suitable to be inserted as username, email, etc.
    This can be used to avoid email/username collisions between test runs
    '''
    return ''.join(random.choices(string.ascii_lowercase, k=32))

def gen_username() -> str:
    ''' Generates a random, valid username. '''
    return f'user-{_gen_identifier()}'

def gen_email() -> str:
    ''' Generates a random, valid email. '''
    return f'{_gen_identifier()}@test.com'


# A generated user, with a valid session ID
class GeneratedSession(NamedTuple):
    username: str
    email: str
    password: str
    sid: str


def _create_session() -> GeneratedSession:
    unq = _gen_identifier()
    data = CreateAccountRequest(username=gen_username(), email=gen_email(), password='Useruser10!')
    res = check_status(requests.post(api_endpoint('create-account'), json=data.model_dump()))
    return GeneratedSession(
        username=data.username,
        email=data.email,
        password=data.password,
        sid=res.cookies['sid']
    )


# Creates a user and a session. This is run at most once per test run.
@pytest.fixture(scope="module")
def session():
    return _create_session()


# Used for tests that require two different users.
@pytest.fixture(scope="module")
def session2():
    return _create_session()
