// Keeping this in an object makes it stable (it's not recreated on each import),
// which is required for mock validation
const router = {
  push: jest.fn(),
  replace: jest.fn(),
};

module.exports = {
  useRouter: jest.fn().mockImplementation(() => router),
};
