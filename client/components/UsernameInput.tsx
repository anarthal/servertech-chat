import { TextField } from "@mui/material";
import { FieldValues, Path, UseFormRegister } from "react-hook-form";

const minLength = 4;

export default function UsernameInput<TFieldValues extends FieldValues>({
  register,
  name,
  errorMessage,
  className = "",
}: {
  register: UseFormRegister<TFieldValues>;
  name: Path<TFieldValues>;
  errorMessage?: string;
  className?: string;
}) {
  return (
    <TextField
      variant="standard"
      label="Username (public)"
      required
      inputProps={{ maxLength: 100, "aria-label": "username" }}
      {...register(name, {
        required: {
          value: true,
          message: "This field is required.",
        },
        minLength: {
          value: minLength,
          message: `Usernames should have at least ${minLength} characters.`,
        },
      })}
      error={!!errorMessage}
      helperText={errorMessage}
      className={className}
    />
  );
}
