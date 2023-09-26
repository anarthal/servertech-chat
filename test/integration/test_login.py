from .conftest import check_status, api_endpoint, GeneratedSession
from .api_types import LoginRequest, APIErrorResponse, ErrorId
import requests
import pytest

def test_success(session: GeneratedSession):
    # We won't be using session.sid, but the generated user
    req_data = LoginRequest(email=session.email, password=session.password)
    res = check_status(requests.post(api_endpoint('login'), json=req_data.model_dump()))
    assert res.status_code == 204 # No data
    assert res.cookies['sid'] != session.sid


def test_wrong_password(session: GeneratedSession):
    req_data = LoginRequest(email=session.email, password='BadPassword!')
    res = requests.post(api_endpoint('login'), json=req_data.model_dump())
    assert res.status_code == 400
    res_data = APIErrorResponse.model_validate(res.json())
    assert res_data.id == ErrorId.LoginFailed


def test_nonexisting_email():
    req_data = LoginRequest(email='bad@test.com', password='BadPassword!')
    res = requests.post(api_endpoint('login'), json=req_data.model_dump())
    assert res.status_code == 400
    res_data = APIErrorResponse.model_validate(res.json())
    assert res_data.id == ErrorId.LoginFailed


@pytest.mark.parametrize("body", [
    {}, # Empty
    { 'email': 10,          'password': 'Useruser10!' }, # email: wrong type
    { 'email': '',          'password': 'Useruser10!' }, # email: empty
    {                       'password': 'Useruser10!' }, # email: missing
    { 'email': 200*'a',     'password': 'Useruser10!' }, # email: too long
    { 'email': 'not-email', 'password': 'Useruser10!' }, # email: bad format
    { 'email': 'non-existing@test.com', 'password': 10 },        # pass: wrong type
    { 'email': 'non-existing@test.com', 'password': '' },        # pass: empty
    { 'email': 'non-existing@test.com', 'password': 'short' },   # pass: too short
    { 'email': 'non-existing@test.com', 'password': 200*'a' },   # pass: too long
    { 'email': 'non-existing@test.com' },                        # pass: missing
], ids=lambda x: str(x))
def test_bad_request(body: dict):
    res = requests.post(api_endpoint('login'), json=body)
    assert res.status_code == 400
    res_data = APIErrorResponse.model_validate(res.json())
    assert res_data.id == ErrorId.BadRequest
