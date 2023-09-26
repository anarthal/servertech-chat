import { render, screen } from "@testing-library/react";
import EmailInput from "@/components/EmailInput";
import "@testing-library/jest-dom";
import { useForm } from "react-hook-form";
import userEvent from "@testing-library/user-event";

type Inputs = { email: string };

const Form = () => {
  const {
    register,
    formState: { errors },
  } = useForm<Inputs>({ mode: "onChange" });
  return (
    <EmailInput
      register={register}
      name="email"
      errorMessage={errors.email?.message}
    />
  );
};

describe("EmailInput", () => {
  test("Success", async () => {
    const user = userEvent.setup();

    // Render
    const { asFragment } = render(<Form />);

    // Rendered correctly
    expect(screen.getByText("Email")).toBeInTheDocument();

    // Type a value
    await user.type(screen.getByLabelText("email"), "test@test.com");

    // No error
    expect(screen.getByDisplayValue("test@test.com")).toBeInTheDocument();
    expect(screen.queryByText("Please enter a valid email.")).toBeNull();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("Invalid email", async () => {
    const user = userEvent.setup();

    // Render
    const { asFragment } = render(<Form />);

    // Type a bad email
    await user.type(screen.getByLabelText("email"), "bad-email");

    // Error message shown
    expect(screen.getByDisplayValue("bad-email")).toBeInTheDocument();
    expect(screen.getByText("Please enter a valid email.")).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });
});
