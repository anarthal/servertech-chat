from .conftest import api_endpoint
import requests

def test_endpoint_not_found():
    # Requesting an endpoint that does not exist returns a 404
    res = requests.post(api_endpoint('bad'))
    assert res.status_code == 404

def test_method_not_allowed():
    # Requesting an endpoint with an unsupported method returns a 405
    res = requests.get(api_endpoint('login'))
    assert res.status_code == 405
