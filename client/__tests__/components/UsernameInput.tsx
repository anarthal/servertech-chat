import { render, screen } from "@testing-library/react";
import UsernameInput from "@/components/UsernameInput";
import "@testing-library/jest-dom";
import { useForm } from "react-hook-form";
import userEvent from "@testing-library/user-event";

type Inputs = { username: string };

const Form = () => {
  const {
    register,
    formState: { errors },
  } = useForm<Inputs>({ mode: "onChange" });
  return (
    <UsernameInput
      register={register}
      name="username"
      errorMessage={errors.username?.message}
    />
  );
};

describe("UsernameInput", () => {
  const tooShortError = "Usernames should have at least 4 characters.";

  test("Success", async () => {
    const user = userEvent.setup();

    // Render
    const { asFragment } = render(<Form />);

    // Rendered correctly
    expect(screen.getByText("Username (public)")).toBeInTheDocument();

    // Type a value
    await user.type(screen.getByLabelText("username"), "somenickname");

    // No error
    expect(screen.getByDisplayValue("somenickname")).toBeInTheDocument();
    expect(screen.queryByText(tooShortError)).toBeNull();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("Username too short", async () => {
    const user = userEvent.setup();

    // Render
    const { asFragment } = render(<Form />);

    // Type a bad email
    await user.type(screen.getByLabelText("username"), "ab");

    // Error message shown
    expect(screen.getByDisplayValue("ab")).toBeInTheDocument();
    expect(screen.getByText(tooShortError)).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });
});
