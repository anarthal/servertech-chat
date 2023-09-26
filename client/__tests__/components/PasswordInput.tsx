import { render, screen } from "@testing-library/react";
import PasswordInput from "@/components/PasswordInput";
import "@testing-library/jest-dom";
import { useForm } from "react-hook-form";
import userEvent from "@testing-library/user-event";

type Inputs = { password: string };

const Form = () => {
  const {
    register,
    formState: { errors },
  } = useForm<Inputs>({ mode: "onChange" });
  return (
    <PasswordInput
      register={register}
      name="password"
      errorMessage={errors.password?.message}
    />
  );
};

describe("PasswordInput", () => {
  const tooShortError = "Passwords should have at least 10 characters.";

  test("Showing/hiding password", async () => {
    const user = userEvent.setup();

    // Render
    const { asFragment } = render(<Form />);

    // Rendered correctly. By default, hidden
    expect(screen.getByText("Password")).toBeInTheDocument();
    expect(screen.getByLabelText("password")).toHaveAttribute(
      "type",
      "password",
    );
    expect(asFragment()).toMatchSnapshot();

    // Type something
    await user.type(screen.getByLabelText("password"), "Useruser10!");
    expect(screen.queryByText(tooShortError)).toBeNull();

    // Show password
    await user.click(screen.getByRole("button"));

    // Password now visible, value still there
    expect(screen.getByDisplayValue("Useruser10!")).toHaveAttribute(
      "type",
      "text",
    );
    expect(asFragment()).toMatchSnapshot();

    // Hide it again
    await user.click(screen.getByRole("button"));

    // Password hidden
    expect(screen.getByDisplayValue("Useruser10!")).toHaveAttribute(
      "type",
      "password",
    );
  });

  test("Password too short", async () => {
    const user = userEvent.setup();

    // Render
    const { asFragment } = render(<Form />);

    // Type a bad email
    await user.type(screen.getByLabelText("password"), "ab");

    // Error message shown
    expect(screen.getByDisplayValue("ab")).toBeInTheDocument();
    expect(screen.getByText(tooShortError)).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });
});
