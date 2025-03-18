import VisibilityIcon from "@mui/icons-material/Visibility";
import VisibilityOffIcon from "@mui/icons-material/VisibilityOff";
import { IconButton, TextField } from "@mui/material";
import { useReducer } from "react";
import { FieldValues, Path, UseFormRegister } from "react-hook-form";

const reducer = (prev: boolean) => !prev;

const minLength = 10;

export default function PasswordInput<TFieldValues extends FieldValues>({
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
  const [showPassword, triggerShowPassword] = useReducer(reducer, false);
  return (
    <div className={className}>
      <TextField
        variant="standard"
        label="Password"
        required
        inputProps={{ maxLength: 100, "aria-label": "password" }}
        InputProps={{
          endAdornment: (
            <IconButton onClick={triggerShowPassword} role="button">
              {showPassword ? <VisibilityOffIcon /> : <VisibilityIcon />}
            </IconButton>
          ),
        }}
        type={showPassword ? "text" : "password"}
        {...register(name, {
          required: {
            value: true,
            message: "This field is required.",
          },
          minLength: {
            value: minLength,
            message: `Passwords should have at least ${minLength} characters.`,
          },
        })}
        error={!!errorMessage}
        helperText={errorMessage}
        style={{ width: "100%" }}
      />
    </div>
  );
}
