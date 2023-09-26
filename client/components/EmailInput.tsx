import { TextField } from "@mui/material";
import { FieldValues, Path, UseFormRegister } from "react-hook-form";

const emailRegex =
  /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;

export default function EmailInput<TFieldValues extends FieldValues>({
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
      label="Email"
      required
      inputProps={{ maxLength: 100, "aria-label": "email" }}
      type="email"
      {...register(name, {
        required: {
          value: true,
          message: "This field is required.",
        },
        pattern: {
          value: emailRegex,
          message: "Please enter a valid email.",
        },
      })}
      error={!!errorMessage}
      helperText={errorMessage}
      className={className}
    />
  );
}
