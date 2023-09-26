from .conftest import api_endpoint, GeneratedSession, gen_email, gen_username
from .api_types import CreateAccountRequest, APIErrorResponse, ErrorId
import requests
import pytest


# The session fixture already tests this case for us
def test_success(session: GeneratedSession):
    assert session.sid != ''


def test_duplicate_username(session: GeneratedSession):
    req_data = CreateAccountRequest(email=gen_email(), username=session.username, password='Useruser10!')
    res = requests.post(api_endpoint('create-account'), json=req_data.model_dump())
    assert res.status_code == 400
    res_data = APIErrorResponse.model_validate(res.json())
    assert res_data.id == ErrorId.UsernameExists


def test_duplicate_email(session: GeneratedSession):
    req_data = CreateAccountRequest(email=session.email, username=gen_username(), password='Useruser10!')
    res = requests.post(api_endpoint('create-account'), json=req_data.model_dump())
    assert res.status_code == 400
    res_data = APIErrorResponse.model_validate(res.json())
    assert res_data.id == ErrorId.EmailExists


@pytest.mark.parametrize("body", [
    # Empty
    {},
    # email: wrong type
    { 'email': 10, 'password': 'Useruser10!', 'username': 'nonexisting-nickname' },
    # email: empty
    { 'email': '', 'password': 'Useruser10!', 'username': 'nonexisting-nickname'  },
    # email: missing
    { 'password': 'Useruser10!', 'username': 'nonexisting-nickname'  },
    # email: too long
    { 'email': 200*'a', 'password': 'Useruser10!', 'username': 'nonexisting-nickname'  },
    # email: bad format
    { 'email': 'not-email', 'password': 'Useruser10!', 'username': 'nonexisting-nickname' },
    # pass: wrong type
    { 'email': 'non-existing@test.com', 'password': 10, 'username': 'nonexisting-nickname' },    
    # pass: empty    
    { 'email': 'non-existing@test.com', 'password': '', 'username': 'nonexisting-nickname' },     
    # pass: too short   
    { 'email': 'non-existing@test.com', 'password': 'short', 'username': 'nonexisting-nickname' },   
    # pass: too long
    { 'email': 'non-existing@test.com', 'password': 200*'a', 'username': 'nonexisting-nickname' },   
    # pass: missing
    { 'email': 'non-existing@test.com', 'username': 'nonexisting-nickname' },   
    # username: wrong type
    { 'email': 'non-existing@test.com', 'password': 'Useruser10!', 'username': 10 },
    # username: empty
    { 'email': 'non-existing@test.com', 'password': 'Useruser10!', 'username': ''  },
    # username: too short
    { 'email': 'non-existing@test.com', 'password': 'Useruser10!', 'username': 'a'  },
    # username: missing
    { 'email': 'non-existing@test.com', 'password': 'Useruser10!' },
    # username: too long
    { 'email': 'non-existing@test.com', 'password': 'Useruser10!', 'username': 200*'a'  },
], ids=lambda x: str(x))
def test_bad_request(body: dict):
    res = requests.post(api_endpoint('create-account'), json=body)
    assert res.status_code == 400
    res_data = APIErrorResponse.model_validate(res.json())
    assert res_data.id == ErrorId.BadRequest
