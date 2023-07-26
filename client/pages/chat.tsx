import Head from 'next/head';
import styles from '../styles/Home.module.css';
import Header from '../components/header';
import { TextField, Button, Avatar } from '@mui/material';
import { useEffect, useState } from 'react';


type User = {
  id: string
  username: string
}

type Message = {
  id: string
  user: User
  content: string
  timestamp: Date
  roomId: string
}

type Room = {
  id: string
  name: string
}

function genRandomName() {
  const number = Math.floor(Math.random() * 999999);
  return `user-${number}`
}

function getStoredUser(): User {
  const user = localStorage.getItem('servertech_user')
  if (user) {
    return JSON.parse(user)
  } else {
    return { id: crypto.randomUUID(), username: genRandomName() }
  }
}

const rooms: Room[] = [
  { id: 'room-1', name: 'Boost.Beast' },
  { id: 'room-2', name: 'Building Boost' },
]

// Copied from MUI Avatar docs
function stringToColor(string: string) {
  let hash = 0;
  let i;

  /* eslint-disable no-bitwise */
  for (i = 0; i < string.length; i += 1) {
    hash = string.charCodeAt(i) + ((hash << 5) - hash);
  }

  let color = '#';

  for (i = 0; i < 3; i += 1) {
    const value = (hash >> (i * 8)) & 0xff;
    color += `00${value.toString(16)}`.slice(-2);
  }
  /* eslint-enable no-bitwise */

  return color;
}

const NameAvatar = ({ name }: { name: string }) => {
  return <Avatar sx={{
    bgcolor: stringToColor(name),
  }}>{name[0]}</Avatar>
}

const RoomEntry = ({ name }: { name: string }) => {
  return (
    <div className='flex p-3'>
      <div className='pr-3 flex flex-col justify-center'>
        <NameAvatar name={name} />
      </div>
      <div className='flex-1 flex flex-col'>
        <p className='m-0 font-bold pb-2'>{name}</p>
        <p className='m-0'>Room last message</p>
      </div>
    </div>
  )
}

export default function Home() {
  const [user, setUser] = useState(null);
  useEffect(() => {
    setUser(getStoredUser());
  }, [])
  return (
    <>
      <Head>
        <title>BoostServerTech chat</title>
        <link rel="icon" href="/favicon.ico" />
      </Head>
      <div className="flex flex-col">
        <Header />
        <div className="flex-1 flex border-">
          <div className='flex-1 flex flex-col'>
            {rooms.map(({ id, name }) => <RoomEntry key={id} name={name} />)}
          </div>
          <div className='flex-[3] flex' style={{ backgroundColor: 'var(--boost-light-grey)' }}>
            <div className='flex-1 flex flex-col p-5'>
              <div className='flex flex-row'>
                <div className='pr-5 flex flex-col justify-end'>
                  <NameAvatar name='User 1' />
                  <p className='m-0 pt-1 text-sm'>User 1</p>
                </div>
                <div className='flex-[6] bg-white rounded-lg pt-4 pl-5 pr-2'>
                  <p className='m-0'>Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message</p>
                  <p className='m-0 pb-2 pr-2 text-xs text-right' style={{ color: 'var(--boost-grey)' }}>12:51</p>
                </div>
                <div className='flex-[3]'>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </>
  )
}
