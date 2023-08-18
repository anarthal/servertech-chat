import { Avatar } from "@mui/material";

// A Google-style avatar with an initial and a color

// Copied from MUI Avatar docs
function stringToColor(string: string) {
  let hash = 0;

  for (let i = 0; i < string.length; i += 1) {
    hash = string.charCodeAt(i) + ((hash << 5) - hash);
  }

  let color = "#";

  for (let i = 0; i < 3; i += 1) {
    const value = (hash >> (i * 8)) & 0xff;
    color += `00${value.toString(16)}`.slice(-2);
  }

  return color;
}

export default function NameAvatar({ name }: { name: string }) {
  return (
    <Avatar
      sx={{
        bgcolor: stringToColor(name),
      }}
    >
      {name[0]}
    </Avatar>
  );
}
